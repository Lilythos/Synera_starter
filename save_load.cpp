#include "save_load.h"
#include "core/bench.h"
#include "core/board.h"
#include "core/equipment.h"
#include "core/game_state.h"
#include "core/player.h"
#include "core/shop.h"
#include "core/synergy_system.h"
#include "entity/heroes.h"

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString phaseToString(GamePhase phase)
{//将游戏阶段枚举转换为字符串，用于存档
    switch (phase) {
    case GamePhase::Prep: return QString::fromUtf8("Prep");
    case GamePhase::Combat: return QString::fromUtf8("Combat");
    case GamePhase::Resolve: return QString::fromUtf8("Resolve");
    }
    return QString::fromUtf8("Prep");
}

GamePhase stringToPhase(const QString& s)
{//将字符串转换回游戏阶段枚举，用于读档
    if (s == QLatin1String("Combat")) return GamePhase::Combat;
    if (s == QLatin1String("Resolve")) return GamePhase::Resolve;
    return GamePhase::Prep;
}

QString ownerToString(Unit::Owner owner)
{//将单位所属阵营枚举转换为字符串
    return owner == Unit::Owner::PlayerCtrl ? QString::fromUtf8("Player") : QString::fromUtf8("Enemy");
}

Unit::Owner stringToOwner(const QString& s)
{//将字符串转换回单位所属阵营枚举
    return s == QLatin1String("Enemy") ? Unit::Owner::EnemyCtrl : Unit::Owner::PlayerCtrl;
}

QString stateToString(Unit::State state)
{//将单位状态枚举转换为字符串，用于存档
    switch (state) {
    case Unit::State::Idle: return QString::fromUtf8("Idle");
    case Unit::State::Moving: return QString::fromUtf8("Moving");
    case Unit::State::Attacking: return QString::fromUtf8("Attacking");
    case Unit::State::Casting: return QString::fromUtf8("Casting");
    case Unit::State::Dead: return QString::fromUtf8("Dead");
    }
    return QString::fromUtf8("Idle");
}

Unit::State stringToState(const QString& s)
{//将字符串转换回单位状态枚举，用于读档
    if (s == QLatin1String("Moving")) return Unit::State::Moving;
    if (s == QLatin1String("Attacking")) return Unit::State::Attacking;
    if (s == QLatin1String("Casting")) return Unit::State::Casting;
    if (s == QLatin1String("Dead")) return Unit::State::Dead;
    return Unit::State::Idle;
}

QString actionToString(Unit::ActionType action)
{//将单位行动类型枚举转换为字符串，用于存档
    switch (action) {
    case Unit::ActionType::None: return QString::fromUtf8("None");
    case Unit::ActionType::Attack: return QString::fromUtf8("Attack");
    case Unit::ActionType::Heal: return QString::fromUtf8("Heal");
    case Unit::ActionType::DeathHeal: return QString::fromUtf8("DeathHeal");
    }
    return QString::fromUtf8("None");
}

Unit::ActionType stringToAction(const QString& s)
{//将字符串转换回单位行动类型枚举，用于读档
    if (s == QLatin1String("Attack")) return Unit::ActionType::Attack;
    if (s == QLatin1String("Heal")) return Unit::ActionType::Heal;
    if (s == QLatin1String("DeathHeal")) return Unit::ActionType::DeathHeal;
    return Unit::ActionType::None;
}

QString equipmentToString(EquipmentType equipment)
{//将装备类型枚举转换为字符串，用于存档
    switch (equipment) {
    case EquipmentType::IronSword: return QString::fromUtf8("IronSword");//铁剑
    case EquipmentType::Chainmail: return QString::fromUtf8("Chainmail");//锁子甲
    case EquipmentType::SwiftGloves: return QString::fromUtf8("SwiftGloves");//迅捷手套
    case EquipmentType::BlueCrystal: return QString::fromUtf8("BlueCrystal");//蓝晶石
    default: return QString::fromUtf8("None");
    }
}

EquipmentType stringToEquipment(const QString& s)
{//将字符串转换回装备类型枚举，用于读档
    if (s == QLatin1String("IronSword")) return EquipmentType::IronSword;
    if (s == QLatin1String("Chainmail")) return EquipmentType::Chainmail;
    if (s == QLatin1String("SwiftGloves")) return EquipmentType::SwiftGloves;
    if (s == QLatin1String("BlueCrystal")) return EquipmentType::BlueCrystal;
    return EquipmentType::None;//其余情况返回无装备
}

QString unitTypeName(Unit* unit)
{//通过动态类型判断获取单位的具体类型名称，用于存档
    if (dynamic_cast<Gunner*>(unit)) return QString::fromUtf8("Gunner");//枪手
    if (dynamic_cast<Healer*>(unit)) return QString::fromUtf8("Healer");//治疗者
    if (dynamic_cast<WarGod*>(unit)) return QString::fromUtf8("WarGod");//战神
    if (dynamic_cast<Martyr*>(unit)) return QString::fromUtf8("Martyr");//殉道者
    if (dynamic_cast<Mimic*>(unit)) return QString::fromUtf8("Mimic");//模仿者
    return QString::fromUtf8("Unit");
}

Unit* createUnitByType(const QString& type)
{//根据存档中记录的类型字符串创建对应的单位实例，用于读档
    if (type == QLatin1String("Gunner")) return new Gunner();
    if (type == QLatin1String("Healer")) return new Healer();
    if (type == QLatin1String("WarGod")) return new WarGod();
    if (type == QLatin1String("Martyr")) return new Martyr();
    if (type == QLatin1String("Mimic")) return new Mimic();
    return new Unit();
}

QJsonObject pointToJson(const QPoint& point)
{//将坐标点转换为JSON对象，用于存档
    QJsonObject obj;//存放坐标信息的JSON对象
    obj["x"] = point.x();//横坐标
    obj["y"] = point.y();//纵坐标
    return obj;
}

QPoint pointFromJson(const QJsonObject& obj)
{//从JSON对象中解析出坐标点，用于读档
    return QPoint(obj.value("x").toInt(-1), obj.value("y").toInt(-1));
}

QJsonObject actionRecordToJson(const QPoint& pos, const Board::ActionRecord& record)
{//将棋盘上某个位置的行动记录转换为JSON对象，用于存档
    QJsonObject obj;//存放行动记录信息的JSON对象
    obj["x"] = pos.x();//横坐标
    obj["y"] = pos.y();//纵坐标
    obj["type"] = actionToString(record.type);//记录行动类型
    obj["amount"] = record.amount;//记录行动数值
    obj["target"] = pointToJson(record.targetPosition);//记录行动的目标位置
    return obj;
}

Board::ActionRecord actionRecordFromJson(const QJsonObject& obj)
{//从JSON对象中解析出行动记录，用于读档
    Board::ActionRecord record;//待还原的行动记录
    record.type = stringToAction(obj.value("type").toString());//还原行动类型
    record.amount = obj.value("amount").toInt(0);//还原行动数值，缺失时默认为0
    //存在合法的目标位置字段才进行还原
    if (obj.contains("target") && obj.value("target").isObject()) { 
        record.targetPosition = pointFromJson(obj.value("target").toObject());
    }
    return record;
}

} // namespace

namespace SaveLoad {

bool saveGameState(const Player& player,
                   const Board& board,
                   const Bench& bench,
                   const Shop& shop,
                   const QList<Unit*>& units,
                   GamePhase phase,
                   int stage,
                   const QString& path)
{//将当前游戏状态保存为JSON文件
    QJsonObject root;//存档文件的根JSON对象

    QJsonObject meta;//存放存档元信息的JSON对象
    meta["version"] = 3;//存档格式版本号，版本3开始保存连胜/连败经济状态
    meta["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);//存档生成的时间戳
    root["meta"] = meta;//将元信息写入根对象

    QJsonObject gameObj;//存放游戏整体状态的JSON对象
    gameObj["phase"] = phaseToString(phase);//记录当前游戏阶段
    gameObj["stage"] = stage;//记录当前关卡数
    root["game"] = gameObj;//将游戏状态写入根对象

    QJsonObject playerObj;//存放玩家信息的JSON对象
    playerObj["hp"] = player.hp();//记录生命值
    playerObj["gold"] = player.gold();//记录金币数量
    playerObj["level"] = player.level();//记录等级
    playerObj["populationCap"] = player.populationCap();//记录人口上限
    playerObj["currentStage"] = player.currentStage();//记录当前关卡
    playerObj["winStreak"] = player.winStreak();//记录当前连胜
    playerObj["lossStreak"] = player.lossStreak();//记录当前连败

    QJsonArray equipmentArr;//存放装备库存的JSON数组
    for (EquipmentType equipment : player.equipmentInventory()) {
        equipmentArr.append(equipmentToString(equipment));//将每件装备转换为字符串后加入数组
    }
    playerObj["equipmentInventory"] = equipmentArr;//将装备库存写入玩家信息
    root["player"] = playerObj;//将玩家信息写入根对象

    QJsonArray unitsArr;//存放所有单位信息的JSON数组
    for (Unit* unit : units) {
        if (!unit) {
            //跳过空指针单位
            continue;
        }

        QJsonObject obj;//存放单个单位信息的JSON对象
        obj["type"] = unitTypeName(unit);//记录单位具体类型
        obj["id"] = unit->id();//记录单位唯一标识
        obj["name"] = unit->name();//记录单位名称
        obj["owner"] = ownerToString(unit->owner());//记录单位所属阵营

        const QPoint boardPos = board.positionOf(unit);//该单位在棋盘上的位置
        if (board.isValidPosition(boardPos)) {
            //单位位于棋盘上，记录棋盘位置信息
            QJsonObject loc;//存放位置信息的JSON对象
            loc["kind"] = QString::fromUtf8("board");//标记位置类型为棋盘
            loc["x"] = boardPos.x();//横坐标
            loc["y"] = boardPos.y();//纵坐标
            obj["location"] = loc;
        } else {
            const int benchIndex = bench.indexOf(unit);//该单位在备战席中的下标
            if (benchIndex != -1) {
                //单位位于备战席，记录备战席位置信息
                QJsonObject loc;//存放位置信息的JSON对象
                loc["kind"] = QString::fromUtf8("bench");//标记位置类型为备战席
                loc["index"] = benchIndex;//记录备战席下标
                obj["location"] = loc;
            } else {
                int shopIndex = -1;//该单位在商店中的下标，初始为未找到
                for (int i = 0; i < Shop::SLOT_COUNT; ++i) {
                    if (shop.getUnitAt(i) == unit) {
                        //找到该单位所在的商店槽位
                        shopIndex = i;
                        break;
                    }
                }

                QJsonObject loc;//存放位置信息的JSON对象
                if (shopIndex != -1) {
                    //单位位于商店，记录商店位置信息
                    loc["kind"] = QString::fromUtf8("shop");//标记位置类型为商店
                    loc["index"] = shopIndex;//记录商店槽位下标
                } else {
                    //未在棋盘/备战席/商店中找到，标记为未放置
                    loc["kind"] = QString::fromUtf8("unplaced");
                }
                obj["location"] = loc;
            }
        }

        obj["baseMaxHp"] = unit->baseMaxHp();//记录单位基础最大生命值
        obj["hp"] = unit->hp();//记录单位当前生命值
        obj["baseAtk"] = unit->baseAtk();//记录单位基础攻击力
        obj["range"] = unit->range();//记录单位攻击范围
        obj["baseMaxMana"] = unit->baseMaxMana();//记录单位基础最大法力值
        obj["mana"] = unit->mana();//记录单位当前法力值
        obj["baseAttackIntervalMs"] = unit->baseAttackIntervalMs();//记录单位基础攻击间隔（ms）
        obj["baseMoveIntervalMs"] = unit->baseMoveIntervalMs();//记录单位基础移动间隔（ms）
        obj["attackElapsedMs"] = unit->attackElapsedMs();//记录单位攻击计时已经过的时间
        obj["moveElapsedMs"] = unit->moveElapsedMs();//记录单位移动计时已经过的时间
        obj["state"] = stateToString(unit->state());//记录单位当前状态
        obj["star"] = unit->star();//记录单位星级

        QJsonArray traitsArr;//存放单位特质的JSON数组
        for (const QString& trait : unit->traits()) {
            traitsArr.append(trait);//将每个特质加入数组
        }
        obj["traits"] = traitsArr;//将特质数组写入单位信息

        obj["equipment"] = equipmentToString(unit->equipment());//记录单位装备
        obj["lastAction"] = actionToString(unit->lastAction());//记录单位上一次行动类型
        obj["acquiredOrder"] = QString::number(unit->acquiredOrder());//记录单位获取顺序

        unitsArr.append(obj);//将该单位信息加入单位数组
    }
    root["units"] = unitsArr;//将所有单位信息写入根对象

    QJsonArray actionArr;//存放棋盘行动记录的JSON数组
    const QHash<QPoint, Board::ActionRecord> records = board.actionRecords();//棋盘上所有位置的行动记录
    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        if (it.value().type != Unit::ActionType::None) {//仅保存有实际行动类型的记录
            actionArr.append(actionRecordToJson(it.key(), it.value()));
        }
    }
    root["boardActions"] = actionArr;//将棋盘行动记录写入根对象

    QFile file(path);//指向存档文件路径的文件对象
    if (!file.open(QIODevice::WriteOnly)) {
        //文件无法以写入模式打开
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));//将根对象转换为带缩进格式的JSON文本并写入文件
    file.close();//关闭文件
    return true;
}

bool loadGameState(Player& player,
                   Board& board,
                   Bench& bench,
                   Shop& shop,
                   QList<Unit*>& ownedUnits,
                   GamePhase& outPhase,
                   int& outStage,
                   const QString& path)
{//从JSON存档文件中读取并还原游戏状态
    QFile file(path);//指向存档文件路径的文件对象
    if (!file.open(QIODevice::ReadOnly)) {
        //文件无法以只读模式打开
        return false;
    }

    QJsonParseError error;//JSON解析错误信息
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);//读取文件全部内容并解析为JSON文档
    file.close();//关闭文件

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        //解析出错或者根节点不是JSON对象
        return false;
    }

    const QJsonObject root = doc.object();//存档文件的根JSON对象

    shop.clear(ownedUnits);//清空商店中的单位
    qDeleteAll(ownedUnits);//释放当前已拥有单位的内存
    ownedUnits.clear();//清空已拥有单位列表
    board.clear();//清空棋盘上的单位
    bench.clear();//清空备战席上的单位
    player.reset();//重置玩家状态为初始值

    //存档中存在合法的玩家信息字段才进行还原
    if (root.contains("player") && root.value("player").isObject()) {
        const QJsonObject obj = root.value("player").toObject();//玩家信息对应的JSON对象
        player.setHp(obj.value("hp").toInt(player.hp()));//还原生命值，缺失时保持当前值
        player.setGold(obj.value("gold").toInt(player.gold()));//还原金币数量
        player.setLevel(obj.value("level").toInt(player.level()));//还原等级
        player.setPopulationCap(obj.value("populationCap").toInt(player.populationCap()));//还原人口上限
        player.setCurrentStage(obj.value("currentStage").toInt(player.currentStage()));//还原当前关卡
        player.setStreaks(obj.value("winStreak").toInt(0), obj.value("lossStreak").toInt(0));//还原连胜/连败状态

        if (obj.contains("equipmentInventory") && obj.value("equipmentInventory").isArray()) {
            //存在合法的装备库存字段才进行还原
            const QJsonArray equipmentArr = obj.value("equipmentInventory").toArray();//装备库存对应的JSON数组
            for (const QJsonValue& value : equipmentArr) {
                player.addEquipment(stringToEquipment(value.toString()));//逐个还原装备并加入玩家库存
            }
        }
    }

    QHash<int, Unit*> savedIdToUnit;//存档中单位ID到新建单位指针的映射，用于后续还原位置

    if (root.contains("units") && root.value("units").isArray()) {
        //存档中存在合法的单位列表字段才进行还原
        const QJsonArray unitsArr = root.value("units").toArray();//单位列表对应的JSON数组

        for (const QJsonValue& value : unitsArr) {
            if (!value.isObject()) {
                //该条目不是合法的JSON对象，跳过
                continue;
            }

            const QJsonObject obj = value.toObject();//单个单位信息对应的JSON对象
            Unit* unit = createUnitByType(obj.value("type").toString(QString::fromUtf8("Unit")));//根据类型字符串创建对应单位实例

            unit->setName(obj.value("name").toString(unit->name()));//还原单位名称
            unit->setOwner(stringToOwner(obj.value("owner").toString()));//还原单位所属阵营
            unit->setMaxHp(obj.value("baseMaxHp").toInt(obj.value("maxHp").toInt(unit->baseMaxHp())));//还原单位基础最大生命值，兼容旧字段名
            unit->setAtk(obj.value("baseAtk").toInt(obj.value("atk").toInt(unit->baseAtk())));//还原单位基础攻击力
            unit->setRange(obj.value("range").toInt(unit->range()));//还原单位攻击范围
            unit->setMaxMana(obj.value("baseMaxMana").toInt(obj.value("maxMana").toInt(unit->baseMaxMana())));//还原单位基础最大法力值
            unit->setAttackIntervalMs(obj.value("baseAttackIntervalMs").toInt(obj.value("attackIntervalMs").toInt(unit->baseAttackIntervalMs())));//还原单位基础攻击间隔
            unit->setMoveIntervalMs(obj.value("baseMoveIntervalMs").toInt(obj.value("moveIntervalMs").toInt(unit->baseMoveIntervalMs())));//还原单位基础移动间隔
            unit->setStar(obj.value("star").toInt(unit->star()));//还原单位星级

            if (obj.contains("traits") && obj.value("traits").isArray()) {
                //存在合法的特质字段才进行还原
                QStringList traits;//待还原的特质列表
                const QJsonArray traitsArr = obj.value("traits").toArray();//特质列表对应的JSON数组
                for (const QJsonValue& trait : traitsArr) {
                    traits.append(trait.toString());//逐个还原特质字符串
                }
                unit->setTraits(traits);//将还原后的特质列表设置给单位
            }

            unit->equip(stringToEquipment(obj.value("equipment").toString()));//还原单位装备
            unit->setHp(obj.value("hp").toInt(unit->hp()));//还原单位当前生命值
            unit->setMana(obj.value("mana").toInt(unit->mana()));//还原单位当前法力值
            unit->setState(stringToState(obj.value("state").toString()));//还原单位当前状态
            unit->setLastAction(stringToAction(obj.value("lastAction").toString()));//还原单位上一次行动类型
            unit->setAttackElapsedMs(obj.value("attackElapsedMs").toInt(0));//还原单位攻击计时已经过的时间
            unit->setMoveElapsedMs(obj.value("moveElapsedMs").toInt(0));//还原单位移动计时已经过的时间
            unit->setAcquiredOrder(obj.value("acquiredOrder").toString().toLongLong());//还原单位获取顺序

            ownedUnits.append(unit);//将还原好的单位加入已拥有单位列表

            const int savedId = obj.value("id").toInt(-1);//该单位在存档中的原始ID
            if (savedId != -1) {
                //存在合法的原始ID才建立映射，便于后续还原位置信息
                savedIdToUnit[savedId] = unit;
            }
        }

        for (const QJsonValue& value : unitsArr) {
            if (!value.isObject()) {
                //该条目不是合法的JSON对象，跳过
                continue;
            }

            const QJsonObject obj = value.toObject();//单个单位信息对应的JSON对象
            const int savedId = obj.value("id").toInt(-1);//该单位在存档中的原始ID
            Unit* unit = savedIdToUnit.value(savedId, nullptr);//根据原始ID查找对应的新建单位
            if (!unit || !obj.contains("location") || !obj.value("location").isObject()) {
                //未找到对应单位，或者没有合法的位置信息
                continue;
            }

            const QJsonObject loc = obj.value("location").toObject();//位置信息对应的JSON对象
            const QString kind = loc.value("kind").toString();//位置类型（棋盘/备战席/商店）

            if (kind == QLatin1String("board")) {
                //该单位原本位于棋盘上
                const QPoint pos(loc.value("x").toInt(-1), loc.value("y").toInt(-1));//还原棋盘坐标
                if (board.isValidPosition(pos)) {
                    board.addUnit(unit, pos);
                }
            } else if (kind == QLatin1String("bench")) {
                //该单位原本位于备战席
                bench.addUnitAt(unit, loc.value("index").toInt(-1));
            } else if (kind == QLatin1String("shop")) {
                //该单位原本位于商店
                shop.placeLoadedUnitAt(unit, loc.value("index").toInt(-1));
            }
        }
    }

    if (root.contains("boardActions") && root.value("boardActions").isArray()) {
        //存档中存在合法的棋盘行动记录字段才进行还原
        const QJsonArray actionArr = root.value("boardActions").toArray();//行动记录列表对应的JSON数组
        for (const QJsonValue& value : actionArr) {
            if (!value.isObject()) {
                //该条目不是合法的JSON对象，跳过
                continue;
            }

            const QJsonObject obj = value.toObject();//单条行动记录对应的JSON对象
            const QPoint pos(obj.value("x").toInt(-1), obj.value("y").toInt(-1));//该行动记录对应的棋盘位置
            board.restoreActionRecord(pos, actionRecordFromJson(obj));//将还原后的行动记录写回棋盘
        }
    }

    if (root.contains("game") && root.value("game").isObject()) {
        //存档中存在合法的游戏状态字段才进行还原
        const QJsonObject obj = root.value("game").toObject();//游戏状态对应的JSON对象
        outPhase = stringToPhase(obj.value("phase").toString());//还原游戏阶段
        outStage = obj.value("stage").toInt(outStage);//还原关卡数，缺失时保持原值
    }

    SynergySystem::refreshSynergyBuffs(board);//读档完成后刷新棋盘上的羁绊增益效果
    return true;
}

} // namespace SaveLoad
