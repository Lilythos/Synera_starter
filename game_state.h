#ifndef GAME_STATE_H
#define GAME_STATE_H

enum class GamePhase {
    Prep,
    Combat,
    Resolve
};

enum class GameResult {
    Ongoing,
    Victory,
    Defeat
};

class GameState
{
public:
    GameState();

    GamePhase phase() const { return m_phase; }
    GameResult result() const { return m_result; }

    bool isOngoing() const { return m_result == GameResult::Ongoing; }
    bool isGameOver() const { return m_result != GameResult::Ongoing; }
    bool isVictory() const { return m_result == GameResult::Victory; }
    bool isDefeat() const { return m_result == GameResult::Defeat; }
    bool isPrepPhase() const { return m_phase == GamePhase::Prep; }
    bool isCombatPhase() const { return m_phase == GamePhase::Combat; }
    bool isResolvePhase() const { return m_phase == GamePhase::Resolve; }

    void reset();
    void setPhase(GamePhase phase);
    GamePhase nextPhase() const;
    void advancePhase();
    void markVictory();
    void markDefeat();

private:
    GamePhase m_phase;
    GameResult m_result;
};

#endif // GAME_STATE_H