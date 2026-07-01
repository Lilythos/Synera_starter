#ifndef CORE_GAME_H
#define CORE_GAME_H

#include <QObject>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <unordered_map>
#include <vector>
#include "bench.h"
#include "board.h"
#include "core/equipment.h"
#include "enemy_generator.h"
#include "game_state.h"
#include "player.h"
#include "shop.h"
#include "hero_factory.h"

class Unit;
class QGraphicsScene;
class GridItem;
class UnitItem;

class Game : public QObject
{
    Q_OBJECT

public:

    explicit Game(QObject* parent = nullptr);
    ~Game();

    void initialize();
    void reset();

    QGraphicsScene* scene() const { return m_scene; }

    Player& player() { return m_player; }//返回玩家数据
    const Player& player() const { return m_player; }

    Bench& bench() { return m_bench; }
    const Bench& bench() const { return m_bench; }

    Board& board() { return m_board; }
    const Board& board() const { return m_board; }

    QList<Unit*>& ownedUnits() { return m_units; }//获取玩家拥有的全部棋子列表
    QList<Unit*> ownedUnits() const { return m_units; }
    void addOwnedUnit(Unit* unit) { if (unit) m_units.append(unit); }//添加新棋子到玩家的单位列表
    void clearOwnedUnits();

    bool saveToFile(const QString& path) const;//存档
    bool loadFromFile(const QString& path);//读档

    GamePhase phase() const;//获取当前游戏阶段
    GameResult result() const;//获取本局结果
    bool isGameOver() const;//判断是否结束

    void setPhase(GamePhase phase);

    //标记成功/失败
    void markVictory();
    void markDefeat();

    void advanceStage();//推进关卡
    bool startCombat();
    bool finishCombat();
    bool finishResolve();
    bool advancePhase();

    void updateCombat(int deltaMs);//更新战斗帧

    int deployedUnitCount() const;//统计棋盘上已放置的己方棋子数量
    bool canDeployMoreUnits() const;//判断当前是否还能往棋盘放棋子

    //备战区与棋盘之间的人员流动
    bool moveBenchUnitToBoard(int benchIndex, const QPoint& boardPos);
    bool moveBoardUnitToBench(const QPoint& boardPos, int benchIndex);
    bool swapBenchUnitWithBoardUnit(int benchIndex, const QPoint& boardPos);

    //商店系统
    Shop& shop() { return m_shop; }
    const Shop& shop() const { return m_shop; }
    bool refreshShop();
    bool purchaseShopUnit(int slotIndex);

    bool upgradePopulation();

    bool processPlayerStarUpgrades();//自动升星系统

    QList<EquipmentType> playerEquipmentInventory() const;//获取玩家背包里所有的装备列表
    bool lastEquipmentDropped() const { return m_lastEquipmentDropped; };//判断上一回合是否掉落了装备
    EquipmentType lastDroppedEquipment() const { return m_lastDroppedEquipment; }//获取上一轮掉落装备
    bool equipUnitFromInventory(int unitId, int inventoryIndex);//取装备
    bool unequipUnitToInventory(int unitId);//卸装备

    QList<Unit*> enemyUnits() const { return m_enemyUnits; }
    int enemyUnitCount() const { return m_enemyUnits.size(); }
    void clearEnemies();
    bool generateEnemiesForCurrentStage();//根据当前关卡，生成对应强度的敌方棋子

    //鼠标拖拽
    void handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);

private:

    enum class CombatRoundResult {
        None,
        PlayerWon,
        PlayerLost
    };

    void createStarterUnitsIfNeeded();//开局初始化生成免费棋子
    void clearAllUnits();
    Unit* findUnitById(int unitId) const;
    GridItem* findGridItem(const QPoint& gridPos) const;
    UnitItem* findUnitItem(int unitId) const;
    void clearGridHighlights();
    bool canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const;
    void applyDrop(int unitId, const QPoint& target);
    void buildScene();
    void syncFromBoard();

    bool hasAlivePlayerUnitsOnBoard() const;//检测是否还有存活的我方棋子
    bool hasAliveEnemyUnitsOnBoard() const;   

    QPointF gridToWorld(int row, int col) const;
    QPoint worldToGrid(const QPointF& world) const;
    QPolygonF cellHexPolygon(int row, int col) const;

    Board m_board;
    Bench m_bench;
    QList<Unit*> m_units;
    QList<Unit*> m_enemyUnits;
    Player m_player;
    GameState m_gameState;//全局游戏状态
    EnemyGenerator m_enemyGenerator;//敌人生成器
    CombatRoundResult m_roundResult;

    QGraphicsScene* m_scene;
    std::vector<GridItem*> m_gridItems;
    std::vector<UnitItem*> m_unitItems;

    Shop m_shop;
    HeroFactory m_heroFactory;
    bool m_lastEquipmentDropped;
    EquipmentType m_lastDroppedEquipment;
    bool addPlayerEquipment(EquipmentType equipmentType);//将装备存入玩家背包
    void awardStageCompletionEquipment();//战斗胜利发放装备奖励

    bool m_dragActive;
    int m_activeUnitId;
    QPoint m_sourceGrid;
    std::unordered_map<int, UnitItem*> m_unitItemById;

    int m_rows;
    int m_cols;
    qreal m_radius;
    qreal m_rowSpacing;
};

#endif // CORE_GAME_H
