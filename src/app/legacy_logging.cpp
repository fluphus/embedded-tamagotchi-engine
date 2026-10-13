// Firmware assembly for the P1 slice. Rules compile without Arduino or Serial.
#include "../config/game_config.h"
#include "../pet/seriousness.h"
#include "diagnostics.h"

namespace {
void legacyLog(const SeriousnessLogEvent &event) {
    switch (event.kind) {
    case SeriousnessLogKind::RhongoStarted:
        app::trace("[Rhongo] Timer STARTED");
        break;
    case SeriousnessLogKind::RhongoTriggered:
        app::trace("[Rhongo] *** TRIGGERED ***");
        break;
    case SeriousnessLogKind::RhongoReset:
        app::tracef("[Rhongo] Timer RESET (SR=%d < %d)\n", event.after, RHONGOMYNIAD_SAFE_DROP);
        break;
    case SeriousnessLogKind::MissedFeedPenalty:
        app::tracef("[Seriousness] Missed feed penalty: %d -> %d (+%d)\n", event.before,
                    event.after, MISSED_FEED_SERIOUSNESS);
        break;
    }
}
} // namespace

// Existing callers keep the same singleton and synchronous logging behavior.
SeriousnessSystem seriousnessSystem(legacyLog);
