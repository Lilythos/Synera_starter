#include "game.h"
#include "core/combat_system.h"
#include "core/synergy_system.h"
#include "core/save_load.h"
#include "entity/heroes.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include "core/star_upgrader.h"
#include "core/equipment.h"
#include <QGraphicsScene>
#include <QtMath>

namespace {
constexpr qreal kZGrid = 0.0;//网格格子的层级（最底层）
constexpr qreal kZUnit = 1.0;//单位的层级
constexpr qreal kZDraggingUnit = 2.0;//正在被拖拽单位的层级（最上层）
constexpr int kVictoryGoldReward = 5;//战斗胜利获得的金币奖励
constexpr int kDefeatGoldReward = 4;//战斗失败仍能获得的金币奖励
constexpr int kDefeatHpPenalty = 10;//战斗失败扣除的玩家生命值
constexpr int kEquipmentDropChancePercent = 40;//通关后掉落装备的概率（百分比）

void resetUnitForPrep(Unit* unit)
{//战斗结束回到准备阶段时，清理单位的战斗临时状态
    if (!unit) {
        return;//空指针不处理
    }

    unit->setState(Unit::State::Idle);//准备阶段不应继续显示攻击/移动/技能状态
    unit->resetCombatTimers();//清空攻击和移动冷却，下一场战斗重新计时
    unit->resetMana();//清空法力，下一场战斗从 0 蓝开始
    unit->setLastAction(Unit::ActionType::None);//清空上一动作，避免模仿者读取上一场残留动作
}
}

Game::Game(QObject* parent)
    : QObject(parent)
    , m_roundResult(CombatRoundResult::None)
    , m_scene(new QGraphicsScene(this))//创建图形场景对象
    , m_lastEquipmentDropped(false)
    , m_lastDroppedEquipment(EquipmentType::None)
    , m_dragActive(false)
    , m_activeUnitId(-1)
    , m_sourceGrid(-1, -1)
    , m_rows(Board::ROWS)
    , m_cols(Board::COLS)
    , m_radius(46.0)
    , m_rowSpacing(69.0)
{}

Game::~Game()
{
    clearAllUnits();
}

void Game::clearOwnedUnits()
{//清空玩家拥有的所有单位
    m_shop.clear(m_units);//清空商店中的单位
    qDeleteAll(m_units);//释放所有单位的内存
    m_units.clear();//清空单位列表
}

bool Game::saveToFile(const QString& path) const
{//将当前游戏状态保存到指定文件
    return SaveLoad::saveGameState(m_player, m_board, m_bench, m_shop, m_units, m_gameState.phase(), m_player.currentStage(), path);
}

bool Game::loadFromFile(const QString& path)
{//从指定文件读取并还原游戏状态
    GamePhase phase = m_gameState.phase();//临时保存当前阶段，作为读档失败时的默认值
    int stage = m_player.currentStage();//临时保存当前关卡，作为读档失败时的默认值
    //执行读档
    bool ok = SaveLoad::loadGameState(m_player, m_board, m_bench, m_shop, m_units, phase, stage, path);
    if (!ok) {
        return false;
    }

    m_gameState.setPhase(phase);//还原游戏阶段
    m_player.setCurrentStage(stage);//还原当前关卡

    m_enemyUnits.clear();//清空敌方单位列表
    for (Unit* unit : m_units) {
        if (unit && unit->owner() == Unit::Owner::EnemyCtrl) {
            //从还原后的单位中找出属于敌方的单位
            m_enemyUnits.append(unit);
        }
    }

    SynergySystem::refreshSynergyBuffs(m_board);//刷新棋盘上的羁绊增益效果
    buildScene();//重新搭建图形场景
    syncFromBoard();//将棋盘状态同步到场景显示
    return true;
}

void Game::initialize()
{
    createStarterUnitsIfNeeded();//创建初始单位
    buildScene();
    reset();
}

void Game::reset()
{
    m_shop.clear(m_units);//清空商店中的单位
    clearEnemies();//清除所有敌方单位
    qDeleteAll(m_units);//释放所有单位的内存
    m_units.clear();//清空单位列表
    m_board.clear();//清空棋盘
    m_bench.clear();//清空备战席

    m_player.reset();//重置玩家属性为初始状态
    createStarterUnitsIfNeeded();//创建初始单位

    m_gameState.reset();//重置游戏状态机
    m_roundResult = CombatRoundResult::None;//重置本局对战结果
    m_lastEquipmentDropped = false;//重置最近一次装备掉落标记
    m_lastDroppedEquipment = EquipmentType::None;//重置最近一次掉落的装备类型
    m_shop.refresh(m_player, m_heroFactory, m_units, false);//刷新商店货架（不收取刷新费用）

    StarUpgrader::processStarUpgrades(m_bench, m_board, m_units, m_player);//处理自动升星

    const QPoint initialPositions[] = {
        QPoint(0, 7),
        QPoint(1, 7),
        QPoint(2, 7)
    };

    for (int i = 0; i < m_units.size() && i < 3; ++i) {
        m_board.addUnit(m_units.at(i), initialPositions[i]);
    }

    SynergySystem::refreshSynergyBuffs(m_board);

    buildScene();
    syncFromBoard();
}

GamePhase Game::phase() const
{
    return m_gameState.phase();
}

GameResult Game::result() const
{
    return m_gameState.result();
}

bool Game::isGameOver() const
{
    return m_gameState.isGameOver();
}

void Game::setPhase(GamePhase phase)
{
    m_gameState.setPhase(phase);
}

void Game::markVictory()
{
    m_gameState.markVictory();
}

void Game::markDefeat()
{
    m_gameState.markDefeat();
}

void Game::advanceStage()
{
    m_player.advanceStage();
    m_gameState.setPhase(GamePhase::Prep);//切换到商店备战阶段

    StarUpgrader::processStarUpgrades(m_bench, m_board, m_units, m_player);//自动升星
}

bool Game::startCombat()
{
    if (isGameOver() || m_gameState.phase() != GamePhase::Prep) {
        return false;
    }

    if (!generateEnemiesForCurrentStage()) {
        return false;
    }

    buildScene();

    m_roundResult = CombatRoundResult::None;//本局胜负状态重置为None

    //羁绊加成
    SynergySystem::refreshSynergyBuffs(m_board);
    SynergySystem::applyPreCombatEffects(m_board);

    m_gameState.setPhase(GamePhase::Combat);

    //禁止拖拽
    m_dragActive = false;
    m_activeUnitId = -1;
    m_sourceGrid = QPoint(-1, -1);
    clearGridHighlights();

    syncFromBoard();

    return true;
}

bool Game::finishCombat()
{
    if (isGameOver() || m_gameState.phase() != GamePhase::Combat) {
        return false;
    }

    //战斗结束后进入结算也不能拖拽棋子
    m_gameState.setPhase(GamePhase::Resolve);
    m_dragActive = false;
    m_activeUnitId = -1;
    m_sourceGrid = QPoint(-1, -1);
    clearGridHighlights();

    return true;
}

bool Game::finishResolve()
{//处理结算阶段，根据本局对战结果推进游戏流程
    if (isGameOver() || m_gameState.phase() != GamePhase::Resolve) {
        return false;
    }

    if (m_roundResult == CombatRoundResult::PlayerWon) {
        //玩家获胜的结算流程
        m_player.recordVictory();//记录连胜，并清空连败
        const int streakBonusGold = m_player.currentStreakBonusGold();//根据当前连胜计算额外金币
        m_player.addGold(kVictoryGoldReward + streakBonusGold);//发放胜利金币和连胜奖励
        awardStageCompletionEquipment();//按概率发放通关装备奖励
        clearEnemies();//清除敌方单位
        QList<Unit*> playerUnits;//存活在棋盘上的玩家单位列表
        for (Unit* unit : m_board.units()) {
            if (unit && unit->owner() == Unit::Owner::PlayerCtrl && unit->isAlive()) {
                //收集存活的玩家单位
                playerUnits.append(unit);
            }
        }
        for (Unit* unit : playerUnits) {
            m_board.removeUnit(unit);//先将这些单位从棋盘上移除
        }
        for (int i = 0; i < playerUnits.size() && i < Board::COLS; ++i) {
            //再将玩家单位重新排列到最后一行站位
            resetUnitForPrep(playerUnits.at(i));//回到准备阶段前重置战斗状态
            m_board.addUnit(playerUnits.at(i), QPoint(i, Board::ROWS - 1));
        }
        m_roundResult = CombatRoundResult::None;//重置本局对战结果
        advanceStage();//进入下一关卡
        syncFromBoard();
        return true;
    }

    if (m_roundResult == CombatRoundResult::PlayerLost) {
        //玩家失败的结算流程
        clearEnemies();//清除敌方单位
        m_player.recordDefeat();//记录连败，并清空连胜
        const int streakBonusGold = m_player.currentStreakBonusGold();//根据当前连败计算额外补偿金币
        m_player.addGold(kDefeatGoldReward + streakBonusGold);//发放失败补给和连败奖励
        m_player.takeDamage(kDefeatHpPenalty);//扣除生命值
        m_lastEquipmentDropped = false;//失败不掉落装备，重置标记
        m_lastDroppedEquipment = EquipmentType::None;//重置最近掉落的装备类型

        if (!m_player.isAlive()) {
            //生命值耗尽，游戏结束
            markDefeat();
            m_roundResult = CombatRoundResult::None;//重置本局对战结果
            syncFromBoard();
            return true;
        }

        m_gameState.setPhase(GamePhase::Prep);
        QList<Unit*> playerUnits;
        for (Unit* unit : m_board.units()) {
            if (unit && unit->owner() == Unit::Owner::PlayerCtrl && unit->isAlive()) {
                playerUnits.append(unit);
            }
        }
        for (Unit* unit : playerUnits) {
            m_board.removeUnit(unit);
        }
        for (int i = 0; i < playerUnits.size() && i < Board::COLS; ++i) {
            resetUnitForPrep(playerUnits.at(i));//回到准备阶段前重置战斗状态
            m_board.addUnit(playerUnits.at(i), QPoint(i, Board::ROWS - 1));
        }
        StarUpgrader::processStarUpgrades(m_bench, m_board, m_units, m_player);
        m_roundResult = CombatRoundResult::None;
        syncFromBoard();
        return true;
    }

    //本局没有产生胜负结果（如平局或异常情况）
    clearEnemies();//清除敌方单位
    m_gameState.setPhase(GamePhase::Prep);//切换回备战阶段
    m_roundResult = CombatRoundResult::None;//重置本局对战结果
    syncFromBoard();
    return true;
}

bool Game::advancePhase()
{//切换流程
    switch (m_gameState.phase()) {
    case GamePhase::Prep:
        return startCombat();
    case GamePhase::Combat:
        return finishCombat();
    case GamePhase::Resolve:
        return finishResolve();
    }

    return false;
}

void Game::updateCombat(int deltaMs)
{
    if (isGameOver() || m_gameState.phase() != GamePhase::Combat || deltaMs <= 0) {
        //游戏结束/当前不在战斗阶段/帧间隔时间无效则不更新
        return;
    }

    CombatSystem::updateBoard(m_board, deltaMs);//交给战斗系统去执行本帧所有战斗行为

    if (!hasAliveEnemyUnitsOnBoard()) {
        m_roundResult = CombatRoundResult::PlayerWon;
        finishCombat();
    } else if (!hasAlivePlayerUnitsOnBoard()) {
        m_roundResult = CombatRoundResult::PlayerLost;
        finishCombat();
    }

    syncFromBoard();
}

bool Game::refreshShop()
{//玩家手动花费金币刷新商店
    if (m_gameState.phase() != GamePhase::Prep) {
        //不在备战阶段不能刷新商店
        return false;
    }

    if (!m_shop.refresh(m_player, m_heroFactory, m_units)) {
        //刷新失败
        return false;
    }

    buildScene();
    syncFromBoard();
    return true;
}

bool Game::purchaseShopUnit(int slotIndex)
{//玩家购买商店指定槛位的单位
    if (m_gameState.phase() != GamePhase::Prep) {
        //不在备战阶段不能购买
        return false;
    }

    if (!m_shop.purchase(slotIndex, m_player, m_bench)) {
        return false;
    }

    StarUpgrader::processStarUpgrades(m_bench, m_board, m_units, m_player);
    SynergySystem::refreshSynergyBuffs(m_board);
    buildScene();
    syncFromBoard();
    return true;
}

bool Game::processPlayerStarUpgrades()
{//手动触发一次升星处理，并判断是否产生了实际变化
    if (m_gameState.phase() != GamePhase::Prep) {
        //不在备战阶段不能升星
        return false;
    }

    const int beforeUnitCount = m_units.size();//升星处理前的单位总数
    int beforeStarTotal = 0;//升星处理前玩家单位的星级总和
    for (Unit* unit : m_units) {
        if (unit && unit->owner() == Unit::Owner::PlayerCtrl) {
            //累加玩家单位的星级
            beforeStarTotal += unit->star();
        }
    }

    StarUpgrader::processStarUpgrades(m_bench, m_board, m_units, m_player);//执行升星处理
    SynergySystem::refreshSynergyBuffs(m_board);
    buildScene();
    syncFromBoard();

    int afterStarTotal = 0;//升星处理后玩家单位的星级总和
    for (Unit* unit : m_units) {
        if (unit && unit->owner() == Unit::Owner::PlayerCtrl) {
            //累加玩家单位的星级
            afterStarTotal += unit->star();
        }
    }

    return m_units.size() != beforeUnitCount || afterStarTotal != beforeStarTotal;//单位数量或星级总和发生变化即认为产生了升星
}

QList<EquipmentType> Game::playerEquipmentInventory() const
{//获取玩家当前的装备库存列表
    return m_player.equipmentInventory();
}

bool Game::equipUnitFromInventory(int unitId, int inventoryIndex)
{//将玩家库存中的装备穿戴到指定单位身上
    if (m_gameState.phase() != GamePhase::Prep) {
        //不在备战阶段不能装备
        return false;
    }

    Unit* unit = findUnitById(unitId);//根据ID查找目标单位
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        //单位不存在或不属于玩家
        return false;
    }

    EquipmentType equipmentType = m_player.removeEquipmentAt(inventoryIndex);//从库存中取出该装备
    if (equipmentType == EquipmentType::None) {
        //该下标没有有效装备
        return false;
    }

    EquipmentType previous = unit->unequip();//先卸下单位原有的装备
    unit->equip(equipmentType);//再装备新取出的装备
    if (previous != EquipmentType::None) {
        //若原来有装备，则放回玩家库存
        m_player.addEquipment(previous);
    }

    return true;
}

bool Game::unequipUnitToInventory(int unitId)
{//将指定单位身上的装备卸下并放回玩家库存
    if (m_gameState.phase() != GamePhase::Prep) {
        return false;
    }

    Unit* unit = findUnitById(unitId);
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        return false;
    }

    EquipmentType previous = unit->equipment();//该单位当前装备的装备类型
    if (previous == EquipmentType::None) {
        return false;
    }

    if (!m_player.addEquipment(previous)) {
        //玩家装备库存已满，无法放入
        return false;
    }

    unit->unequip();//卸下单位身上的装备
    return true;
}

bool Game::upgradePopulation()
{//玩家花费金币提升人口上限
    if (m_gameState.phase() != GamePhase::Prep) {
        //不在备战阶段不能提升人口
        return false;
    }

    return m_player.upgradePopulation(Player::POPUPGRADE_COST);
}

void Game::handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF&)
{//处理玩家开始拖拽单位的事件
    if (m_gameState.phase() != GamePhase::Prep) {
        //不在备战阶段不能拖拽
        return;
    }

    Unit* unit = findUnitById(unitId);
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        return;
    }

    m_dragActive = true;//标记当前正在进行拖拽
    m_activeUnitId = unitId;//记录正在拖拽的单位ID
    m_sourceGrid = sourceGrid;//记录拖拽起始的网格坐标

    UnitItem* item = findUnitItem(unitId);//找到该单位对应的图形项
    if (item) {
        //将其层级提升到最上层，便于拖拽时显示在最前面
        item->setZValue(kZDraggingUnit);
    }
}

void Game::handleDragMoved(int unitId, const QPoint&, const QPointF& scenePos)
{//处理拖拽过程中鼠标移动的事件
    if (!m_dragActive) {
        //当前没有进行中的拖拽
        return;
    }

    clearGridHighlights();//清除之前的网格高亮状态

    const QPoint target = worldToGrid(scenePos);//将场景坐标转换为目标网格坐标
    GridItem* targetItem = findGridItem(target);//找到目标网格对应的图形项
    if (!targetItem) {
        //未找到对应网格
        return;
    }

    targetItem->setHoverActive(true);//高亮显示当前悬停的网格

    if (canApplyDrop(unitId, m_sourceGrid, target)) {
        //若该位置可以放置，进一步显示可放置高亮
        targetItem->setDropActive(true);
    }
}

void Game::handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos)
{//处理玩家释放拖拽的事件
    if (!m_dragActive) {
        //当前没有进行中的拖拽
        return;
    }

    const QPoint target = worldToGrid(scenePos);

    clearGridHighlights();
    if (canApplyDrop(unitId, sourceGrid, target)) {
        applyDrop(unitId, target);
    }

    UnitItem* item = findUnitItem(m_activeUnitId);//找到该单位对应的图形项
    if (item) {
        //恢复其原本的层级
        item->setZValue(kZUnit);
    }

    m_dragActive = false;//结束拖拽状态
    m_activeUnitId = -1;//清空正在拖拽的单位ID
    m_sourceGrid = QPoint(-1, -1);//清空拖拽起始坐标

    syncFromBoard();//将棋盘状态同步到场景显示
}

bool Game::addPlayerEquipment(EquipmentType equipmentType)
{//向玩家装备库存添加一件装备
    return m_player.addEquipment(equipmentType);
}

void Game::awardStageCompletionEquipment()
{//通关后按概率发放装备奖励
    m_lastEquipmentDropped = false;//先重置最近一次装备掉落标记
    m_lastDroppedEquipment = EquipmentType::None;//先重置最近掉落的装备类型

    if (!rollEquipmentDrop(kEquipmentDropChancePercent)) {
        //本次概率判定未掉落装备
        return;
    }

    EquipmentType drop = randomEquipmentDrop();//随机抽取一件掉落装备
    if (addPlayerEquipment(drop)) {
        //成功加入玩家库存才记录掉落信息
        m_lastEquipmentDropped = true;
        m_lastDroppedEquipment = drop;
    }
}

void Game::createStarterUnitsIfNeeded()
{//若玩家还没有任何单位，则创建初始的三个单位
    if (!m_units.isEmpty()) {
        return;
    }

    m_units.append(new Gunner());//枪手
    m_units.append(new Healer());//治疗师
    m_units.append(new Martyr());//殉道者

    for (Unit* unit : m_units) {
        if (unit) {
            //为每个初始单位记录获取顺序
            unit->setAcquiredOrder(m_player.nextAcquiredOrder());
        }
    }
}

void Game::clearAllUnits()
{//清空游戏中所有单位
    m_shop.clear(m_units);//清空商店中的单位
    clearEnemies();//清除所有敌方单位
    qDeleteAll(m_units);//释放所有单位的内存
    m_units.clear();//清空单位列表
    m_bench.clear();//清空备战席
    m_board.clear();//清空棋盘
}

Unit* Game::findUnitById(int unitId) const
{//根据单位ID在所有单位中查找对应实例
    for (Unit* unit : m_units) {
        if (unit && unit->id() == unitId) {
            //找到ID匹配的单位
            return unit;
        }
    }
    return nullptr;
}

GridItem* Game::findGridItem(const QPoint& gridPos) const
{//根据网格坐标查找对应的网格图形项
    for (GridItem* item : m_gridItems) {
        if (item && item->gridPos() == gridPos) {
            //找到坐标匹配的网格项
            return item;
        }
    }
    return nullptr;
}

UnitItem* Game::findUnitItem(int unitId) const
{//根据单位ID查找对应的单位图形项
    auto it = m_unitItemById.find(unitId);//在映射表中查找该ID
    if (it == m_unitItemById.end()) {
        return nullptr;
    }
    return it->second;//返回找到的图形项
}

void Game::clearGridHighlights()
{//清除所有网格的高亮状态
    for (GridItem* item : m_gridItems) {
        if (!item) {
            continue;
        }
        item->setHoverActive(false);//取消悬停高亮
        item->setDropActive(false);//取消可放置高亮
    }
}

bool Game::canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const
{//判断指定单位是否可以从起点拖拽放置到目标位置
    if (m_gameState.phase() != GamePhase::Prep) {
        return false;
    }
    Unit* unit = findUnitById(unitId);
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        return false;
    }

    if (!m_board.isValidPosition(source) || !m_board.isValidPosition(target)) {
        //起点或目标坐标不是棋盘合法位置
        return false;
    }

    if (!m_board.isPlayerHalf(target)) {
        //目标位置不在玩家半场，不可放置
        return false;
    }

    if (source == target || m_board.hasUnitAt(target)) {
        //目标与起点相同，或目标位置已有单位
        return false;
    }

    return m_board.getUnitAt(source) == unit;//确认起点位置上的单位确实是被拖拽的单位
}

void Game::applyDrop(int unitId, const QPoint& target)
{//将指定单位从原位置移动到目标位置
    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return;
    }

    m_board.removeUnit(unit);//先将单位从棋盘原位置移除
    m_board.addUnit(unit, target);//再将单位添加到目标位置
}

void Game::buildScene()
{//根据当前棋盘和单位数据重新搭建图形场景
    m_scene->clear();//清空场景中所有图形项
    m_gridItems.clear();//清空网格图形项列表
    m_unitItems.clear();//清空单位图形项列表
    m_unitItemById.clear();//清空单位ID到图形项的映射

    QRectF totalBounds;//所有网格的整体边界范围
    bool first = true;//标记是否是第一个网格，用于初始化边界范围
    for (int row = 0; row < Board::ROWS; ++row) {
        for (int col = 0; col < Board::COLS; ++col) {
            const QPolygonF poly = cellHexPolygon(row, col);//计算该格子对应的六边形多边形
            GridItem* gridItem = new GridItem(row, col, poly);//创建该格子的图形项
            gridItem->setZValue(kZGrid);//设置网格层级为最底层
            gridItem->setBaseColor(row < Board::ROWS / 2 ? QColor(80, 60, 60) : QColor(60, 60, 80));//上半区与下半区使用不同的底色区分敌我半场

            m_scene->addItem(gridItem);//将网格图形项加入场景
            m_gridItems.push_back(gridItem);//记录到网格图形项列表

            const QRectF bounds = gridItem->boundingRect();//该网格图形项的边界
            totalBounds = first ? bounds : totalBounds.united(bounds);//累加合并整体边界范围
            first = false;//标记已处理过第一个网格
        }
    }

    for (Unit* unit : m_units) {
        UnitItem* unitItem = new UnitItem(unit);//为每个单位创建对应的图形项
        unitItem->setZValue(kZUnit);//设置单位层级
        m_scene->addItem(unitItem);//将单位图形项加入场景
        m_unitItems.push_back(unitItem);//记录到单位图形项列表
        m_unitItemById[unit->id()] = unitItem;//记录单位ID到图形项的映射

        connect(unitItem, &UnitItem::dragStarted,
                this, &Game::handleDragStarted);//连接拖拽开始信号
        connect(unitItem, &UnitItem::dragMoved,
                this, &Game::handleDragMoved);//连接拖拽移动信号
        connect(unitItem, &UnitItem::dragDropped,
                this, &Game::handleDropCommand);//连接拖拽放置信号
    }

    m_scene->setSceneRect(totalBounds.adjusted(-40, -40, 40, 40));//设置场景显示范围，并在四周留出一定边距
}

void Game::syncFromBoard()
{//将棋盘上单位的实际状态同步到图形场景的显示效果
    clearGridHighlights();//先清除网格高亮状态

    for (UnitItem* item : m_unitItems) {
        if (!item || !item->unit()) {
            //图形项或对应单位为空，跳过
            continue;
        }

        const QPoint pos = item->unit()->position();//该单位在棋盘上记录的位置
        if (!m_board.isValidPosition(pos) || m_board.getUnitAt(pos) != item->unit()) {
            //该位置不合法或棋盘该位置上的单位并非该单位
            item->setVisible(false);//隐藏该图形项
            continue;
        }

        item->setVisible(true);//确保图形项可见
        item->setGridPos(pos);//更新图形项记录的网格坐标
        item->setPos(gridToWorld(pos.y(), pos.x()));//根据网格坐标计算场景坐标并更新位置
        item->setZValue(kZUnit);//恢复单位的正常层级
    }
}

QPointF Game::gridToWorld(int row, int col) const
{//将网格坐标转换为场景中的实际像素坐标
    const qreal colSpacing = m_radius * qSqrt(3.0);//相邻列之间的水平间距
    const qreal xOffset = (row % 2 == 0) ? colSpacing * 0.5 : 0.0;//偶数行整体错位偏移，形成蜂窝状排列
    const qreal x = xOffset + col * colSpacing;//该格子的横坐标
    const qreal y = row * m_rowSpacing;//该格子的纵坐标
    return QPointF(x, y);
}

QPoint Game::worldToGrid(const QPointF& world) const
{//将场景中的实际像素坐标转换为最接近的网格坐标
    QPoint best(-1, -1);//记录距离最近的网格坐标，初始为无效值
    qreal bestDist = 1e18;//记录当前最小距离的平方，初始为一个极大值

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            const QPointF center = gridToWorld(row, col);//该网格对应的场景坐标
            const qreal dx = world.x() - center.x();//横坐标差值
            const qreal dy = world.y() - center.y();//纵坐标差值
            const qreal d2 = dx * dx + dy * dy;//距离的平方
            if (d2 < bestDist) {
                //发现更近的网格，更新最优结果
                bestDist = d2;
                best = QPoint(col, row);
            }
        }
    }

    return best;//返回距离最近的网格坐标
}

QPolygonF Game::cellHexPolygon(int row, int col) const
{//计算指定网格位置对应的六边形多边形顶点
    const QPointF center = gridToWorld(row, col);//该格子的中心场景坐标
    QPolygonF poly;//存放六边形顶点的多边形
    poly.reserve(6);//预留6个顶点的空间

    for (int i = 0; i < 6; ++i) {
        const qreal angleDeg = 60.0 * i - 90.0;//第i个顶点对应的角度（度），从正上方开始每60度一个顶点
        const qreal angleRad = qDegreesToRadians(angleDeg);//将角度转换为弧度
        poly.append(QPointF(
            center.x() + m_radius * qCos(angleRad),//该顶点的横坐标
            center.y() + m_radius * qSin(angleRad)//该顶点的纵坐标
        ));
    }

    return poly;//返回组成六边形的所有顶点
}

int Game::deployedUnitCount() const
{//统计当前棋盘上玩家已部署的单位数量
    int count = 0;//已部署单位计数
    for (Unit* unit : m_board.units()) {
        if (unit && unit->owner() == Unit::Owner::PlayerCtrl) {
            //该单位属于玩家，计数加一
            ++count;
        }
    }
    return count;
}

bool Game::hasAlivePlayerUnitsOnBoard() const
{//判断棋盘上是否还存在存活的玩家单位
    for (Unit* unit : m_board.units()) {
        if (unit && unit->owner() == Unit::Owner::PlayerCtrl && unit->isAlive()) {
            //找到一个存活的玩家单位
            return true;
        }
    }

    return false;//未找到任何存活的玩家单位
}

bool Game::hasAliveEnemyUnitsOnBoard() const
{//判断棋盘上是否还存在存活的敌方单位
    for (Unit* unit : m_board.units()) {
        if (unit && unit->owner() == Unit::Owner::EnemyCtrl && unit->isAlive()) {
            //找到一个存活的敌方单位
            return true;
        }
    }

    return false;//未找到任何存活的敌方单位
}

bool Game::canDeployMoreUnits() const
{//判断玩家是否还能继续部署更多单位
    return deployedUnitCount() < m_player.populationCap();
}

bool Game::moveBenchUnitToBoard(int benchIndex, const QPoint& boardPos)
{//将备战席上指定下标的单位移动到棋盘指定位置
    if (m_gameState.phase() != GamePhase::Prep) {
        return false;
    }

    Unit* unit = m_bench.getUnitAt(benchIndex);//备战席该下标处的单位
    if (!unit) {
        //该下标没有单位
        return false;
    }

    if (!m_board.isValidPosition(boardPos) || !m_board.isPlayerHalf(boardPos)) {
        return false;
    }

    Unit* boardUnit = m_board.getUnitAt(boardPos);//目标位置上原本的单位
    if (boardUnit) {
        if (boardUnit->owner() != Unit::Owner::PlayerCtrl) {
            //目标位置上的单位不属于玩家，不能交换
            return false;
        }
        return swapBenchUnitWithBoardUnit(benchIndex, boardPos);//目标位置有玩家单位，执行交换
    }

    if (!canDeployMoreUnits()) {
        //目标位置为空但玩家已达人口上限，不能部署
        return false;
    }

    m_bench.removeUnitAt(benchIndex);//从备战席移除该单位
    m_board.addUnit(unit, boardPos);//将单位添加到棋盘目标位置
    syncFromBoard();//
    return true;
}

bool Game::moveBoardUnitToBench(const QPoint& boardPos, int benchIndex)
{//将棋盘上指定位置的单位移动到备战席指定下标
    if (m_gameState.phase() != GamePhase::Prep) {
        return false;
    }

    Unit* unit = m_board.getUnitAt(boardPos);//棋盘该位置上的单位
    if (!unit) {
        return false;
    }

    if (!m_bench.isValidIndex(benchIndex) || m_bench.hasUnitAt(benchIndex)) {
        //目标备战席下标不合法，或该位置已有单位
        return false;
    }

    m_board.removeUnit(unit);//先将单位从棋盘移除
    if (!m_bench.addUnitAt(unit, benchIndex)) {
        //添加到备战席失败，将单位放回原棋盘位置以保持状态一致
        m_board.addUnit(unit, boardPos);
        return false;
    }

    syncFromBoard();
    return true;
}

bool Game::swapBenchUnitWithBoardUnit(int benchIndex, const QPoint& boardPos)
{//交换备战席单位与棋盘上指定位置的单位
    if (m_gameState.phase() != GamePhase::Prep) {
        return false;
    }
    Unit* benchUnit = m_bench.getUnitAt(benchIndex);//备战席该下标处的单位
    Unit* boardUnit = m_board.getUnitAt(boardPos);//棋盘该位置处的单位

    if (!benchUnit || !boardUnit) {
        //任意一方为空都无法交换
        return false;
    }

    if (!m_board.isValidPosition(boardPos) || !m_board.isPlayerHalf(boardPos)) {
        return false;
    }

    m_bench.removeUnitAt(benchIndex);//从备战席移除该单位
    m_board.removeUnit(boardUnit);//从棋盘移除该单位

    m_board.addUnit(benchUnit, boardPos);//将原备战席单位放到棋盘位置
    if (!m_bench.addUnitAt(boardUnit, benchIndex)) {
        //放入备战席失败
        m_board.removeUnit(benchUnit);
        m_board.addUnit(boardUnit, boardPos);
        return false;
    }
    syncFromBoard();
    return true;
}

void Game::clearEnemies()
{//清除当前所有敌方单位
    bool removedAny = false;//标记是否实际移除了任何单位
    for (Unit* enemy : m_enemyUnits) {
        if (!enemy) {
            continue;
        }
       
        m_board.removeUnit(enemy);//从棋盘移除该敌方单位
        m_units.removeAll(enemy);//从总单位列表移除
        delete enemy;//释放该单位的内存
        removedAny = true;//标记已移除过单位
    }

    m_enemyUnits.clear();//清空敌方单位列表
    if (removedAny) {
        buildScene();
        syncFromBoard();
    }
}

bool Game::generateEnemiesForCurrentStage()
{//根据玩家当前关卡生成对应的敌方单位
    clearEnemies();//先清除已有的敌方单位

    const QVector<EnemySpawn> spawns = m_enemyGenerator.generate(m_player.currentStage());//根据当前关卡生成敌人生成配置列表
    bool generatedAny = false;//标记是否成功生成了任意敌人

    for (const EnemySpawn& spawn : spawns) {
        if (!m_board.isValidPosition(spawn.position)) {
            //生成位置不是棋盘合法位置
            continue;
        }

        if (m_board.isPlayerHalf(spawn.position) || m_board.hasUnitAt(spawn.position)) {
            continue;
        }

        Unit* enemy = new Unit(spawn.name,
                               spawn.maxHp,
                               spawn.atk,
                               spawn.range,
                               spawn.maxMana,
                               Unit::Owner::EnemyCtrl,
                               spawn.traits,
                               1);//根据生成配置创建敌方单位实例

        m_enemyUnits.append(enemy);//加入敌方单位列表
        m_units.append(enemy);//加入总单位列表
        m_board.addUnit(enemy, spawn.position);//将敌方单位放置到棋盘对应位置
        generatedAny = true;//标记已成功生成单位
    }

    syncFromBoard();//将棋盘状态同步到场景显示
    return generatedAny;//返回是否成功生成了至少一个敌方单位
}
