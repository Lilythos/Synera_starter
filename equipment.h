#ifndef CORE_EQUIPMENT_H
#define CORE_EQUIPMENT_H

#include <QString>
#include <QList>
#include <QVector>

enum class EquipmentType {
    None,
    IronSword,
    Chainmail,
    SwiftGloves,
    BlueCrystal
};

struct EquipmentInfo {
    EquipmentType type;
    QString name;
    int atkBonus;
    int maxHpBonus;
    int maxManaBonus;
    int attackIntervalReductionMs;
    int attackSpeedPercentBonus;
};

const EquipmentInfo& equipmentInfo(EquipmentType type);
QList<EquipmentType> allEquipmentTypes();
EquipmentType randomEquipmentDrop();
bool rollEquipmentDrop(int chancePercent);
QString equipmentName(EquipmentType type);

#endif // CORE_EQUIPMENT_H
