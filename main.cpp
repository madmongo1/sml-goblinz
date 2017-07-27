/*
 * copyright (c) 2017 Richard Hodges
 *
 * You are free to use, modify and play with this amazing game as much as you wish.
 *
 * If you use the techniques learned here, please cite me. I may need a job one day!
 *
 */

#include <boost/sml.hpp>
#include <iostream>
#include <boost/asio.hpp>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace sml = boost::sml;
namespace asio = boost::asio;

/** Notify the goblin's state that he's been born and given a name
 *
 */
struct birth {
    std::string name;
};

/** notify the goblin that he's dying
 *
 */
struct die {
};

/** notify the goblin that his memory has been forgotten. He will be removed from the battlefield
 *
 */
struct forget {
};

/** The goblin informs himself that he has killed a stinking human
 *
 */
struct kill_a_pleb {
};

/** The goblin thinks to himself "hmm. I'd like to kill again"
 * This is a hack to get around a perceived limitation in the transition table, in that the first match
 * is executed, not subsequent matches of the same event in the same state
 */
struct try_to_kill_again {};


/** Vital details about a goblin
 *
 */
struct goblin_character_sheet {
    std::string name;
    int kill_count;
};

/** Separation of concerns. This class contains the mechanics of how a goblin interacts with the battlefield
 *
 */
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

/** Action which names a goblin upon birth
 *
 */
auto be_named = [](goblin_character_sheet &sheet, birth event) {
    sheet.name = std::move(event.name);
    std::cout << sheet.name << " lives! grarrh!" << std::endl;
};

/** Action indicating that a goblin has been forgotten. Remove his vile carcass from the battlefield!
 *
 */
auto forget_me = [](goblin_io &io, goblin_character_sheet& cs) {
    std::cout << cs.name << " has been forgotten..." << std::endl;
    io.set_done();
};

/** Get our your sword and kill stinking humans!
 *
 */
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

/** The goblin marks his scorecard
 *
 */
auto score_kill = [](goblin_character_sheet &cs) {
    std::cout << "yarrh! another dead!" << std::endl;
    ++cs.kill_count;
};

/** Has the goblin killed sufficient numbers of humans to be considered for a place in Goblin Valhalla?
 *
 */
auto enough_dead = [](goblin_character_sheet &cs) -> bool {
    std::cout << "enough dead?" << std::endl;
    return cs.kill_count >= 5;
};

/** Announce the sad news that the goblin has died, and make preparations to forget he ever existed
 *
 */
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

/** A goblin's modus operandi
 *
 */
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

    /*
     * Where we will schedule goblin events
     */
    asio::io_service executor;
    asio::io_service::work work(executor);

    /*
     * do the whole thing on another thread
     */

    std::thread t{[&] { executor.run(); }};

    /*
     * build a goblin character sheet...
     */

    auto gobbo_details = goblin_character_sheet();

    /*
     * ... and an execution state to interface nicely with the execution environment...
     */

    goblin_io gobbo_exec{executor};

    /*
     * behold! our goblin is conceived
     */

    auto gobbo = sml::sm<goblin_state>(gobbo_exec, gobbo_details);

    /*
     * let's name our baby and send him on hit atrocious way
     */

    executor.dispatch([&] {
        gobbo.process_event(birth{"gobbo"});
    });

    /*
     * wait until the goblin has been forgotten
     */

    gobbo_exec.wait();

    /*
     * ...and shut down nicely ...
     */

    executor.stop();
    t.join();
}