#include <boost/sml.hpp>
#include <iostream>
#include <boost/asio.hpp>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace sml = boost::sml;
namespace asio = boost::asio;

struct birth {
    std::string name;
};
struct die {
};
struct forget {
};

struct kill_a_pleb {
};
struct try_to_kill_again {};


struct goblin_character_sheet {
    std::string name;
    int kill_count;
};

struct goblin_io {
    goblin_io(asio::io_service &exec) : executor_(exec) {}


    auto get_lock() { return std::unique_lock<std::mutex>(m); }

    auto set_done() {
        auto lock = get_lock();
        done = true;
        lock.unlock();
        cv.notify_all();
    }

    auto wait() {
        auto lock = get_lock();
        cv.wait(lock, [this] { return this->done; });
    }

    asio::io_service &executor_;
    asio::io_service::strand strand{executor_};
    asio::deadline_timer kill_timer{executor_};
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
};

auto be_named = [](goblin_character_sheet &sheet, birth event) {
    sheet.name = std::move(event.name);
    std::cout << sheet.name << " lives! grarrh!" << std::endl;
};

auto forget_me = [](goblin_io &io, goblin_character_sheet& cs) {
    std::cout << cs.name << " has been forgotten..." << std::endl;
    io.set_done();
};

auto start_killin = [](auto &&event, auto &&sm, auto &&deps, auto &&subs) {
    auto &io = sml::aux::get<goblin_io &>(deps);
    io.kill_timer.expires_from_now(boost::posix_time::milliseconds(200));
    io.kill_timer.async_wait(io.strand.wrap([&](auto err) {
        if (not err) {
            sm.process_event(kill_a_pleb(), deps, subs);
            sm.process_event(try_to_kill_again(), deps, subs);
        }
    }));
};

auto score_kill = [](goblin_character_sheet &cs) {
    std::cout << "yarrh! another dead!" << std::endl;
    ++cs.kill_count;
};

auto enough_dead = [](goblin_character_sheet &cs) -> bool {
    std::cout << "enough dead?" << std::endl;
    return cs.kill_count >= 5;
};

auto query = [](goblin_character_sheet &cs) -> bool {
    std::cout << "query?" << std::endl;
    return true;
};

auto announce_death = [](auto &&event, auto &&sm, auto &&deps, auto &&subs)
{
    auto& cs = sml::aux::get<goblin_character_sheet&>(deps);
    auto& io = sml::aux::get<goblin_io&>(deps);

    std::cout << cs.name << " died after killin' " << cs.kill_count << " smelly 'umans" << std::endl;
    io.kill_timer.expires_from_now(boost::posix_time::milliseconds(1000));
    io.kill_timer.async_wait(io.strand.wrap([&](auto err){
        if (not err) {
            sm.process_event(forget(), deps, subs);
        }
    }));
};

struct goblin_state {
    auto operator()() const {
        using namespace sml;

        auto be_born = event<birth>;
        auto be_dead = event<die>;
        auto be_forgotten = event<forget>;
        auto yarrgh = event<kill_a_pleb>;
        auto kill_again = event<try_to_kill_again>;

        auto unborn = state<class unborn>;
        auto killing_folk = state<class killing_folk>;
        auto dead = state<class dead>;

        return make_transition_table(
                *unborn + be_born / (be_named, start_killin) = killing_folk,

                killing_folk + yarrgh / (score_kill),
                killing_folk + kill_again [!enough_dead] / start_killin,
                killing_folk + kill_again [enough_dead] / [] {} = dead,
                killing_folk + be_dead = dead,

                dead + on_entry<_> / announce_death,

                dead + be_forgotten / forget_me = X
        );
    }

};


int main() {
    asio::io_service executor;
    asio::io_service::work work(executor);
    std::thread t{[&] { executor.run(); }};

    auto gobbo_details = goblin_character_sheet();
    goblin_io gobbo_exec{executor};
    auto gobbo = sml::sm<goblin_state>(gobbo_exec, gobbo_details);
    executor.dispatch([&] {
        gobbo.process_event(birth{"gobbo"});
    });


    gobbo_exec.wait();
    executor.stop();
    t.join();
}