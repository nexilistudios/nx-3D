#include <client/gamemode/bomb_player_state.hh>

int main()
{
    using nx3d::client::gamemode::isBombPlayerEliminated;
    using nx3d::client::gamemode::shouldShowEliminated;
    using nx3d::client::gamemode::canRequestBombEquip;
    if (shouldShowEliminated(0, false, false)) // Just joined.
        return 1;
    if (shouldShowEliminated(0, true, false)) // Admitted, waiting for first round.
        return 1;
    if (shouldShowEliminated(1, false, false)) // Queued spectator.
        return 1;
    if (shouldShowEliminated(1, true, true)) // Spawned.
        return 1;
    if (!shouldShowEliminated(1, true, false)) // Killed during a round.
        return 1;
    if (isBombPlayerEliminated(0, false, false, false)) // Just joined and can move.
        return 1;
    if (isBombPlayerEliminated(0, true, false, false)) // Waiting for round one.
        return 1;
    if (isBombPlayerEliminated(1, false, false, true)) // Queued spectator.
        return 1;
    if (isBombPlayerEliminated(1, true, true, false)) // Alive in a round.
        return 1;
    if (!isBombPlayerEliminated(1, true, false, false)) // Eliminated by server state.
        return 1;
    if (!isBombPlayerEliminated(1, true, true, true)) // Fatal damage arrived first.
        return 1;
    if (canRequestBombEquip(false, false, "waiting")) // Just selected a team.
        return 1;
    if (canRequestBombEquip(false, false, "live")) // Queued during a round.
        return 1;
    if (canRequestBombEquip(true, false, "live")) // Eliminated.
        return 1;
    if (canRequestBombEquip(true, true, "post")) // Between rounds.
        return 1;
    if (!canRequestBombEquip(true, true, "live")) // Admitted and spawned.
        return 1;
    return 0;
}
