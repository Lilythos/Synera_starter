#ifndef HERO_FACTORY_H
#define HERO_FACTORY_H

class Unit;

class HeroFactory
{
public:
    HeroFactory();
    Unit* createRandomHero() const;
    Unit* createRandomCombatHero() const;
};

#endif // HERO_FACTORY_H
