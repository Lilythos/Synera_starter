#include "hero_factory.h"
#include "entity/heroes.h"
#include <QRandomGenerator>


HeroFactory::HeroFactory() = default;

Unit* HeroFactory::createRandomHero() const
{//从完整英雄池中随机创建一个英雄
    const int options = 5;
    const int choice = int(QRandomGenerator::global()->bounded(options));

    switch (choice) {
    case 0:
        return new Gunner();//枪手
    case 1:
        return new Healer();//治疗师
    case 2:
        return new WarGod();//战神
    case 3:
        return new Martyr();//殉道者
    default:
        return new Mimic();//模仿者
    }
}

Unit* HeroFactory::createRandomCombatHero() const
{//从偏战斗英雄池中随机创建一个英雄，用于商店保底战斗位
    const int options = 3;
    const int choice = int(QRandomGenerator::global()->bounded(options));

    switch (choice) {
    case 0:
        return new Gunner();
    case 1:
        return new WarGod();
    default:
        return new Martyr();
    }
}
