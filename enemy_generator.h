#ifndef ENEMY_GENERATOR_H
#define ENEMY_GENERATOR_H

#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>

struct EnemySpawn
{
    QString name;
    int maxHp;
    int atk;
    int range;
    int maxMana;
    QPoint position;
    QStringList traits;
};

class EnemyGenerator
{
public:
    QVector<EnemySpawn> generate(int stage) const;
};

#endif // ENEMY_GENERATOR_H