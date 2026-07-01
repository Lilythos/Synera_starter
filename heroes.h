#ifndef HEROES_H
#define HEROES_H

#include "unit.h"

class Gunner : public Unit
{
public:
    Gunner();

    int basicAttackDamage() override;
    bool castSkill(Board& board, const QVector<Unit*>& units) override;

private:
    int m_attackCount;
};

class Healer : public Unit
{
public:
    Healer();

    bool castSkill(Board& board, const QVector<Unit*>& units) override;
    void onAfterMoveStep(Board& board, const QVector<Unit*>& units) override;

private:
    int m_moveStepCount;
};

class WarGod : public Unit
{
public:
    WarGod();
    bool isWarGod() const override { return true; }
    bool castSkill(Board& board, const QVector<Unit*>& units) override;
    void onAfterMoveStep(Board& board, const QVector<Unit*>& units) override;

private:
    int m_moveStepCount;
};

class Martyr : public Unit
{
public:
    Martyr();

    void onAfterMoveStep(Board& board, const QVector<Unit*>& units) override;
    void onDeath(Board& board, const QVector<Unit*>& units) override;

private:
    int m_moveStepCount;
};

class Mimic : public Unit
{
public:
    Mimic();

    bool onBeforeCombatAction(Board& board, const QVector<Unit*>& units) override;
};

#endif // HEROES_H
