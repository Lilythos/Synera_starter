#include "game_state.h"

GameState::GameState()
{
    reset();
}

void GameState::reset()
{
    m_phase = GamePhase::Prep;
    m_result = GameResult::Ongoing;
}

void GameState::setPhase(GamePhase phase)
{//设置当前阶段
    m_phase = phase;
}

GamePhase GameState::nextPhase() const
{
    switch (m_phase) {
    case GamePhase::Prep:
        return GamePhase::Combat;
    case GamePhase::Combat:
        return GamePhase::Resolve;
    case GamePhase::Resolve:
        return GamePhase::Prep;
    }

    return GamePhase::Prep;
}

void GameState::advancePhase()
{//推进到下一个阶段
    m_phase = nextPhase();
}

void GameState::markVictory()
{
    m_result = GameResult::Victory;
}

void GameState::markDefeat()
{
    m_result = GameResult::Defeat;
}
