#include "equipment.h"
#include <QRandomGenerator>

const EquipmentInfo& equipmentInfo(EquipmentType type)
{//根据装备类型查找装备数值配置
    static const QVector<EquipmentInfo> infos = {
        { EquipmentType::IronSword, QString::fromUtf8("铁剑"), 15, 0, 0, 0, 0 },//铁剑：增加攻击力
        { EquipmentType::Chainmail, QString::fromUtf8("锁子甲"), 0, 150, 0, 0, 0 },//锁子甲：增加最大生命值
        { EquipmentType::SwiftGloves, QString::fromUtf8("迅捷手套"), 0, 0, 0, 0, 20 },//迅捷手套：提升攻击速度
        { EquipmentType::BlueCrystal, QString::fromUtf8("蓝晶石"), 0, 0, -30, 0, 0 }//蓝晶石：降低最大法力值
    };

    for (const EquipmentInfo& info : infos) {
        //找到类型匹配的装备配置
        if (info.type == type) {
            return info;
        }
    }
    
    static const EquipmentInfo noneInfo = { EquipmentType::None, QString::fromUtf8("无"), 0, 0, 0, 0, 0 };
    return noneInfo;
}

QList<EquipmentType> allEquipmentTypes()
{//返回所有可掉落/可使用的基础装备类型
    return {
        EquipmentType::IronSword,
        EquipmentType::Chainmail,
        EquipmentType::SwiftGloves,
        EquipmentType::BlueCrystal
    };
}

EquipmentType randomEquipmentDrop()
{//从基础装备池中随机选择一件装备
    const QList<EquipmentType> types = allEquipmentTypes();//所有可随机掉落的装备
    const int index = int(QRandomGenerator::global()->bounded(types.size()));//随机下标
    return types.at(index);//返回随机到的装备类型
}

bool rollEquipmentDrop(int chancePercent)
{//根据百分比概率判断是否掉落装备
    if (chancePercent <= 0) {
        return false;
    }
    if (chancePercent >= 100) {
        return true;
    }
    return int(QRandomGenerator::global()->bounded(100)) < chancePercent;//0-99 随机数小于概率则掉落
}

QString equipmentName(EquipmentType type)
{//获取装备显示名称
    return equipmentInfo(type).name;
}
