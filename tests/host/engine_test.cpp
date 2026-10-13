#include "app/day_rules.h"
#include "app/save_format.h"
#include "app/time_rules.h"
#include "app/serial_codec.h"
#include "engine/slot_store.h"
#include "presentation/animation_queue.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
namespace {
int checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::cerr << __func__ << ':' << __LINE__ << " failed: " #x "\n";                       \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
struct Clock {
    uint32_t epoch = 1700000000, mono = 1000, random = 999, calls = 0;
};
engine::Ports ports(Clock &c) {
    return {&c, [](void *p) -> engine::EpochSeconds { return {static_cast<Clock *>(p)->epoch}; },
            [](void *p) -> engine::MonotonicMillis { return {static_cast<Clock *>(p)->mono}; },
            [](void *p) -> uint32_t {
                auto &c = *static_cast<Clock *>(p);
                ++c.calls;
                return c.random;
            },
            nullptr};
}
PetState fresh() {
    PetState p{};
    p.initNew(1700000000);
    return p;
}
void lifecycle() {
    Clock clock;
    app::configure(ports(clock));
    auto p = fresh();
    engine::Command init;
    init.kind = engine::CommandKind::Initialize;
    init.time = {clock.epoch};
    app::execute(p, init);
    CHECK(content::knownForm(p.form));
    CHECK(selected::validate(selected::capture(p)));
    uint8_t picked[3] = {0, 1, 3};
    const auto feed = feedingSystem.feed(p, picked, clock.epoch, 8);
    CHECK(feed.result == FEED_OK);
    CHECK(p.daily_feed.feed_count == 1);
    CHECK(p.health > 50);
    CHECK(feedingSystem.feed(p, picked, clock.epoch + 1799, 8).result == FEED_ERR_TOO_SOON);
    CHECK(feedingSystem.feed(p, picked, clock.epoch + 1800, 8).result == FEED_OK);
    CHECK(feedingSystem.feed(p, picked, clock.epoch + 3600, 8).result == FEED_OK);
    CHECK(feedingSystem.feed(p, picked, clock.epoch + 5400, 8).result == FEED_ERR_DAILY_LIMIT);
    p.seriousness = 50;
    seriousnessSystem.onInteract(p, INTERACT_POKE, clock.epoch + 5400);
    CHECK(p.seriousness < 50);
    seriousnessSystem.onIdleBatch(p, 1440, clock.epoch + 91800);
    CHECK(p.seriousness <= 100);
    const auto child = p.form;
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    p.age_days = 1;
    CHECK(sprout::read(selected::capture(p)).water >= 6);
#else
    p.age_days = 3;
#endif
    const auto evolved = evolutionSystem.checkChildGraduation(p);
    CHECK(evolved.event != EVO_NONE);
    CHECK(p.stage == STAGE_ADULT);
    CHECK(p.form != child);
    GalleryData g;
    g.init();
    g.unlock(child);
    g.unlock(p.form);
    CHECK(g.getUnlockedCount() == 2);
    SaveHeader hdr{};
    hdr.sequence = 17;
    hdr.save_time = clock.epoch + 91800;
    uint8_t bytes[engine::MaxSaveBytes];
    size_t size = 0;
    CHECK(app::encodeSave(p, hdr, &g, bytes, sizeof(bytes), size));
    auto restored = fresh();
    SaveHeader loaded{};
    GalleryData restoredGallery;
    bool full = false;
    CHECK(app::decodeSave(bytes, size, restored, loaded, &restoredGallery, &full) ==
          engine::DecodeError::Ok);
    CHECK(full && loaded.sequence == 17 && restoredGallery.getUnlockedCount() == 2);
    CHECK(restored.form == p.form && restored.age_days == p.age_days &&
          restored.health == p.health);
    uint8_t again[engine::MaxSaveBytes];
    size_t againSize = 0;
    CHECK(app::encodeSave(restored, loaded, &restoredGallery, again, sizeof(again), againSize));
    CHECK(size == againSize && std::equal(bytes, bytes + size, again));
    app::resetPet(restored, clock.epoch + 100000, 329);
    CHECK(restored.age_days == 0);
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    CHECK(restored.form == 40);
    CHECK(!restored.is_nobu);
    CHECK(clock.calls == 0);
#else
    CHECK(restored.is_nobu && restored.form == FORM_NOBU);
    CHECK(clock.calls == 0); // forced route consumes no RNG
#endif
    auto state = selected::capture(p);
    engine::Engine<selected::Policy> one(ports(clock)), two(ports(clock));
    auto copy = state;
    engine::Command poke;
    poke.kind = engine::CommandKind::Poke;
    poke.time = {clock.epoch + 100000};
    one.execute(state, poke);
    two.execute(copy, poke);
    CHECK(state.seriousness == copy.seriousness && state.extension == copy.extension);
    CHECK(one.saveDue());
    one.markSaved();
    CHECK(!one.saveDue());
    clock.epoch -= 100000;
    CHECK(!one.saveDue());
    clock.mono += 299999;
    CHECK(!one.saveDue());
    ++clock.mono;
    CHECK(one.saveDue());
    engine::Event event;
    size_t events = 0;
    while (one.poll(event))
        ++events;
    CHECK(events >= 1);
    CHECK(!one.eventOverflowed());
    for (int i = 0; i < 20; ++i) {
        one.execute(state, poke);
    }
    CHECK(one.eventOverflowed());
    one.discardEvents();
    CHECK(!one.eventOverflowed());
}
void formats() {
    engine::Document doc;
    doc.state = selected::capture(fresh());
    doc.savedAt = 1700000000;
    doc.sequence = 3;
    doc.full = true;
    doc.gallery.unlock(doc.state.form);
    const auto pack = app::packageContract();
    std::array<uint8_t, engine::MaxSaveBytes> bytes{};
    size_t size = 0;
    CHECK(engine::encode(doc, pack, bytes.data(), bytes.size(), size) == engine::DecodeError::Ok);
    engine::Document output;
    output.sequence = 123;
    auto reject = [&](engine::DecodeError expected, const auto &damaged, size_t n) {
        CHECK(engine::decode(damaged.data(), n, pack, output) == expected);
        CHECK(output.sequence == 123);
    };
    for (size_t n = 0; n < size; ++n)
        if (n != engine::PetBytes)
            reject(engine::DecodeError::Length, bytes, n);
    auto change = [&](size_t offset, uint8_t v, bool repair) {
        auto b = bytes;
        b[offset] = v;
        if (repair) {
            auto crc = engine::recordChecksum(b.data(), size);
            b[22] = crc & 255;
            b[23] = crc >> 8;
        }
        return b;
    };
    reject(engine::DecodeError::Version, change(4, 99, false), size);
    reject(engine::DecodeError::Package, change(8, 0, false), size);
    reject(engine::DecodeError::Checksum, change(30, 99, false), size);
    reject(engine::DecodeError::UnknownId, change(24, 254, true), size);
    reject(engine::DecodeError::Extension, change(51, 2, true), size);
    reject(engine::DecodeError::Extension, change(53, 65, true), size);
    reject(engine::DecodeError::UnknownId, change(161, 254, true), size);
    CHECK(engine::decode(bytes.data(), size, pack, output) == engine::DecodeError::Ok);
    CHECK(output.state.extension == doc.state.extension);
    uint8_t small[4] = {1, 2, 3, 4};
    size_t unused = 0;
    CHECK(engine::encode(doc, pack, small, sizeof(small), unused) == engine::DecodeError::Length);
    CHECK(small[0] == 1);
}
struct Memory {
    std::array<std::vector<uint8_t>, 6> records;
    bool failStage = false, corruptStage = false, failCommit = false;
    static size_t read(void *context, uint8_t slot, bool staged, uint8_t *out, size_t capacity) {
        auto &m = *static_cast<Memory *>(context);
        auto bytes = m.records[slot + (staged ? 3 : 0)];
        if (staged && m.corruptStage && !bytes.empty())
            bytes.back() ^= 1;
        if (bytes.size() > capacity)
            return 0;
        std::copy(bytes.begin(), bytes.end(), out);
        return bytes.size();
    }
    static bool write(void *context, uint8_t slot, bool staged, const uint8_t *bytes, size_t size) {
        auto &m = *static_cast<Memory *>(context);
        if ((staged && m.failStage) || (!staged && m.failCommit))
            return false;
        m.records[slot + (staged ? 3 : 0)] = {bytes, bytes + size};
        return true;
    }
};
void slots() {
    Memory memory;
    engine::SlotStore store({&memory, Memory::read, Memory::write}, app::packageContract());
    engine::Document d;
    d.state = selected::capture(fresh());
    d.sequence = 1;
    d.savedAt = 1700000000;
    CHECK(store.write(0, d));
    d.sequence = 2;
    CHECK(store.write(1, d));
    const auto good = memory.records;
    engine::Document restored;
    uint8_t loaded = 255;
    CHECK(store.loadPair(0, 1, restored, loaded) && loaded == 1 && restored.sequence == 2);
    memory.records[1][22] ^= 1;
    CHECK(store.loadPair(0, 1, restored, loaded) && loaded == 0 && restored.sequence == 1);
    memory.records = good;
    for (int failure = 0; failure < 3; ++failure) {
        memory.failStage = failure == 0;
        memory.corruptStage = failure == 1;
        memory.failCommit = failure == 2;
        d.sequence = 3;
        CHECK(!store.write(0, d));
        CHECK(memory.records[0] == good[0] && memory.records[1] == good[1]);
    }
    memory.failStage = memory.corruptStage = memory.failCommit = false;
    d.state.form = 254;
    CHECK(!store.write(0, d));
    CHECK(memory.records[0] == good[0]);
    // owner slot 0 frozen; the active guest pair is 1/2 and must not write slot 0.
    d.state = selected::capture(fresh());
    d.sequence = 4;
    CHECK(store.write(2, d));
    CHECK(memory.records[0] == good[0]);
    CHECK(store.loadPair(1, 2, restored, loaded) && loaded == 2);
}
void legacy() {
    std::ifstream stream("tests/fixtures/v9/adult-white.full.bin", std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), {});
    CHECK(bytes.size() == 84);
    engine::Document d;
    CHECK(artoria::migrateV9(bytes.data(), bytes.size(), d) == engine::DecodeError::Ok);
    CHECK(d.state.form == 3 && d.state.health == 73 && d.state.seriousness == 100 &&
          d.state.age_days == 3);
    CHECK(d.state.idle_minute_remainder == 4 && d.state.idle_paused_until == 1700001800 &&
          d.state.last_poke_effect_time == 1700000000);
    CHECK(d.sequence == 42 && d.savedAt == 1700000000 && d.gallery.size == 4);
    auto p = artoria::project(d.state);
    CHECK(p.mapo_tofu_count == 2 && p.white_fun_form_locked &&
          p.white_fun_form == FORM_WHITE_ARCHER);
    CHECK(p.daily_feed.feed_count == 2 && p.daily_feed.junk_outside_window == 1 &&
          p.rhongo_timer_start == 1699900000);
    PetState view = fresh();
    SaveHeader hdr{};
    GalleryData gallery;
    bool full = false;
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    CHECK(app::decodeSave(bytes.data(), bytes.size(), view, hdr, &gallery, &full) ==
          engine::DecodeError::Package);
#else
    CHECK(app::decodeSave(bytes.data(), bytes.size(), view, hdr, &gallery, &full) ==
          engine::DecodeError::Ok);
    uint8_t modern[engine::MaxSaveBytes];
    size_t n = 0;
    CHECK(app::encodeSave(view, hdr, &gallery, modern, sizeof(modern), n));
    auto migrated = fresh();
    CHECK(app::decodeSave(modern, n, migrated, hdr, &gallery, &full) == engine::DecodeError::Ok);
    CHECK(migrated.mapo_tofu_count == 2 && migrated.daily_feed.feed_count == 2);
#endif
    for (size_t size = 0; size < 84; ++size)
        if (size != 72)
            CHECK(artoria::migrateV9(bytes.data(), size, d) == engine::DecodeError::Length);
    for (const char *name : {"nobu", "black", "rhongo"}) {
        std::ifstream f(std::string("tests/fixtures/v9/") + name + ".pet.bin", std::ios::binary);
        std::vector<uint8_t> fixture((std::istreambuf_iterator<char>(f)), {});
        CHECK(fixture.size() == 72);
        CHECK(artoria::migrateV9(fixture.data(), fixture.size(), d) == engine::DecodeError::Ok);
        const auto old = artoria::project(d.state);
        CHECK((old.is_nobu && old.form == FORM_NOBU) ||
              (old.is_black_rhongomyniad && old.mapo_tofu_count == 3) ||
              (old.is_rhongomyniad && old.form == FORM_WHITE_LANCER_RHONGOMYNIAD));
    }
    bytes[20] ^= 1;
    CHECK(artoria::migrateV9(bytes.data(), bytes.size(), d) == engine::DecodeError::Checksum);
}
void calendarAndVisit() {
    Clock clock;
    app::configure(ports(clock));
    auto pet = fresh();
    app::initializePet(pet, clock.epoch + 600);
    CHECK(pet.birth_timestamp == clock.epoch + 600 && pet.age_days == 0);
    pet.seriousness = 20;
    pet.idle_paused_until = clock.epoch + 1800;
    const auto day = app::settleDay(pet, clock.epoch + 86400, 15);
    CHECK(pet.age_days == 1 && pet.daily_feed.feed_count == 0 && pet.daily_feed.date_day == 15);
    CHECK(pet.idle_paused_until == 0 && day.idle.seriousness_after == 100);
    const auto plan = app::offlinePlan(2 * 86400 + 125);
    CHECK(plan.days == 2 && plan.minutes == 2);
    CHECK(app::offlinePlan(59).days == 0 && app::offlinePlan(59).minutes == 0);
    for (uint32_t i = 0; i < plan.days; ++i)
        app::settleDay(pet, clock.epoch + (i + 2) * 86400, uint8_t(16 + i));
    CHECK(pet.age_days == 3);
    pet.idle_paused_until = clock.epoch + 1800;
    pet.last_poke_effect_time = clock.epoch;
    pet.rhongo_timer_start = clock.epoch;
    pet.birth_timestamp = clock.epoch - 86400;
    pet.last_interact_time = clock.epoch;
    pet.daily_feed.last_feed_time = clock.epoch;
    const auto frozen = pet;
    realignOwnerTimestamps(pet, 3600);
    CHECK(pet.birth_timestamp == frozen.birth_timestamp + 3600 &&
          pet.idle_paused_until == frozen.idle_paused_until + 3600);
    CHECK(pet.rhongo_timer_start == frozen.rhongo_timer_start + 3600);
    CHECK(app::frozenDuration(clock.epoch, clock.epoch + 3600) == 3600);
    CHECK(app::frozenDuration(clock.epoch, clock.epoch - 1) == 0 &&
          app::frozenDuration(0, clock.epoch) == 0);
    realignImportedTimestamps(pet, clock.epoch, clock.epoch + 3600, 18);
    CHECK(pet.daily_feed.feed_count == 0 && pet.daily_feed.date_day == 18);
    CHECK(pet.idle_paused_until == clock.epoch + 3600); // original future timestamp clamp retained
    auto small = frozen;
    realignImportedTimestamps(small, clock.epoch, clock.epoch + 59, 19);
    CHECK(small.birth_timestamp == frozen.birth_timestamp && small.daily_feed.date_day == 19);
    auto far = frozen;
    realignImportedTimestamps(far, 1, clock.epoch, 20);
    CHECK(far.birth_timestamp == frozen.birth_timestamp && far.daily_feed.date_day == 20);
}
void packageRules() {
    Clock clock;
    app::configure(ports(clock));
    auto p = fresh();
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    p.age_days = 1;
    CHECK(evolutionSystem.checkChildGraduation(p).event == EVO_NONE);
    uint8_t dew[3] = {0, 0, 0};
    CHECK(feedingSystem.feed(p, dew, clock.epoch, 8).result == FEED_OK);
    CHECK(evolutionSystem.checkChildGraduation(p).event == EVO_FORM_CHANGED && p.form == 47);
    CHECK(FOOD_TABLE[0].id == 17 && content::FormCount == 2);
#else
    uint8_t healthy[3] = {0, 1, 3};
    p.seriousness = 50;
    auto feed = feedingSystem.feed(p, healthy, clock.epoch, 8);
    CHECK(p.health == 81 && p.seriousness == 35 && feed.health_from_combo == 15);
    seriousnessSystem.onInteract(p, INTERACT_FEED, clock.epoch);
    CHECK(p.seriousness == 20);
    p = fresh();
    p.age_days = 3;
    p.health = 49;
    evolutionSystem.checkChildGraduation(p);
    CHECK(p.alignment == ALIGN_BLACK);
    p = fresh();
    p.age_days = 3;
    p.health = 50;
    evolutionSystem.checkChildGraduation(p);
    CHECK(p.alignment == ALIGN_WHITE);
    auto state = selected::capture(p);
    state.age_days = 5;
    clock.random = 249;
    CHECK(selected::Policy::resetOption(state, 1, ports(clock)) == 1);
    clock.random = 250;
    CHECK(selected::Policy::resetOption(state, 1, ports(clock)) == 0);
    state.age_days = 4;
    clock.random = 9;
    CHECK(selected::Policy::resetOption(state, 1, ports(clock)) == 1);
    clock.random = 10;
    CHECK(selected::Policy::resetOption(state, 1, ports(clock)) == 0);
    p = fresh();
    p.daily_feed.healthy_in_window = 2;
    p.daily_feed.junk_outside_window = 2;
    auto day = feedingSystem.processDayEnd(p);
    CHECK(day.window_bonus_applied && day.window_penalty_applied && day.missed_feed_penalty &&
          p.health == 50);
    p = fresh();
    p.stage = STAGE_ADULT;
    p.alignment = ALIGN_WHITE;
    p.rhongo_timer_start = clock.epoch;
    p.seriousness = 99;
    seriousnessSystem.onIdleTick(p, clock.epoch + 172800);
    CHECK(!p.is_rhongomyniad);
    p.seriousness = 100;
    seriousnessSystem.onIdleTick(p, clock.epoch + 172801);
    CHECK(p.is_rhongomyniad);
#endif
}
void serialAndCapacity() {
    auto pet=fresh();engine::Command command;command.kind=engine::CommandKind::Feed;
    command.time={1700000000};command.hour=8;command.foods={256,257,258};
    const auto before=selected::capture(pet);
    CHECK(app::execute(pet,command).feed.result!=FEED_OK);
    CHECK(selected::capture(pet).health==before.health && pet.daily_feed.feed_count==0);
    command.kind=engine::CommandKind::SpecialFood;command.option=256;
    app::execute(pet,command);CHECK(selected::capture(pet).health==before.health);

#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    auto saturated=selected::capture(fresh());auto water=sprout::read(saturated);water.water=UINT16_MAX;sprout::write(saturated,water);
    pet=selected::project(saturated);uint8_t wet[3]={0,1,2};
    CHECK(feedingSystem.feed(pet,wet,1700000000,8).result==FEED_OK);
    CHECK(sprout::read(selected::capture(pet)).water==UINT16_MAX);
    FeedOutcome reward{};reward.combo_triggered=true;feedingSystem.applySpecialFood(pet,reward,0);
    CHECK(sprout::read(selected::capture(pet)).water==UINT16_MAX);
#endif
    uint8_t bytes[16]{};size_t size=0;
    CHECK(decodeHexToBytes("00aAFF",bytes,sizeof(bytes),&size)&&size==3&&bytes[0]==0&&bytes[1]==170&&bytes[2]==255);
    CHECK(!decodeHexToBytes("",bytes,sizeof(bytes),&size));CHECK(!decodeHexToBytes("0",bytes,sizeof(bytes),&size));
    CHECK(!decodeHexToBytes("GG",bytes,sizeof(bytes),&size));CHECK(!decodeHexToBytes("0000",bytes,1,&size));
    engine::Gallery gallery;for(uint16_t i=0;i<32;++i)CHECK(gallery.unlock(i));CHECK(!gallery.unlock(32)&&gallery.size==32);
}
void animations() {
    Clock clock;
    app::configure(ports(clock));
    AnimationQueue q;
    q.init();
    AnimNode node;
    node.durationMs = 500;
    CHECK(q.enqueue(node));
    CHECK(q.isPlaying());
    q.update(clock.mono + 499);
    CHECK(q.size() == 1);
    q.update(clock.mono + 500);
    CHECK(q.isEmpty());
    node.type = NODE_WAIT_INPUT;
    node.durationMs = 0;
    CHECK(q.enqueue(node));
    q.update(clock.mono + 10000);
    CHECK(q.isWaitingInput());
    q.skipCurrent();
    CHECK(q.isWaitingInput());
    q.resumeFromInput();
    CHECK(q.isEmpty());
    node.type = NODE_PET_POKE;
    node.durationMs = 500;
    for (int i = 0; i < 16; ++i)
        CHECK(q.enqueue(node));
    CHECK(!q.enqueue(node) && q.overflowed());
    q.clear();
    CHECK(!q.overflowed());
    auto p = fresh();
    p.seriousness = 50;
    const auto before = selected::capture(p);
    CHECK(q.enqueue(node));
    q.update(clock.mono + 500);
    CHECK(selected::capture(p).extension == before.extension &&
          p.seriousness == 50); // playback does not execute rules
}
} // namespace
int main() {
    lifecycle();
    formats();
    slots();
    legacy();
    calendarAndVisit();
    packageRules();
    serialAndCapacity();
    animations();
    std::cout << "PASS " << content::Title << ": " << checks << " checks\n";
}
