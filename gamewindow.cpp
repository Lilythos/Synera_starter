#include "gamewindow.h"
#include "core/board.h"
#include "core/equipment.h"
#include "core/game.h"
#include "core/shop.h"
#include "core/synergy_system.h"
#include "gui/unititem.h"
#include "entity/unit.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace {

QString phaseText(GamePhase phase)
{//把阶段枚举转换成中文显示文本
    switch (phase) {
    case GamePhase::Prep:
        return QString::fromUtf8("准备");//准备阶段
    case GamePhase::Combat:
        return QString::fromUtf8("战斗");//战斗阶段
    case GamePhase::Resolve:
        return QString::fromUtf8("结算");//结算阶段
    }

    return QString::fromUtf8("未知");
}

QString advanceButtonText(GamePhase phase)
{//根据当前阶段决定主按钮文字
    switch (phase) {
    case GamePhase::Prep:
        return QString::fromUtf8("开始战斗");//准备阶段点击后进入战斗
    case GamePhase::Combat:
        return QString::fromUtf8("进入结算");//战斗阶段可手动进入结算
    case GamePhase::Resolve:
        return QString::fromUtf8("下一回合");//结算阶段点击后进入下一轮准备
    }

    return QString::fromUtf8("下一阶段");
}

QPoint sceneToGrid(const QPointF& scenePos)
{//把场景坐标转换为最近的棋盘格坐标
    constexpr qreal radius = 46.0;//棋盘格半径，需和 Game 中绘制参数一致
    constexpr qreal rowSpacing = 69.0;//行间距
    const qreal colSpacing = radius * qSqrt(3.0);//列间距

    QPoint best(-1, -1);//当前找到的最近格子
    qreal bestDist = 1e18;//当前最小距离平方

    for (int row = 0; row < Board::ROWS; ++row) {
        const qreal xOffset = (row % 2 == 0) ? colSpacing * 0.5 : 0.0;//偶数行水平偏移
        for (int col = 0; col < Board::COLS; ++col) {
            const QPointF center(xOffset + col * colSpacing, row * rowSpacing);//当前格中心点
            const qreal dx = scenePos.x() - center.x();//x 方向距离
            const qreal dy = scenePos.y() - center.y();//y 方向距离
            const qreal dist = dx * dx + dy * dy;//距离平方，避免开方
            if (dist < bestDist) {
                bestDist = dist;//更新最小距离
                best = QPoint(col, row);//记录最近格子
            }
        }
    }

    return best;//返回最接近鼠标位置的棋盘格
}

bool hasAliveUnitForOwner(const Board& board, Unit::Owner owner)
{//判断棋盘上某一方是否还有存活单位
    for (Unit* unit : board.units()) {
        if (unit && unit->owner() == owner && unit->isAlive()) {
            return true;//找到对应阵营存活单位
        }
    }

    return false;//没有找到存活单位
}

QString signedNumberText(int value)
{//生成带正负号的数值文本，避免 QString::arg 不支持 "%+1" 的写法
    return value > 0
               ? QString::fromUtf8("+%1").arg(value)
               : QString::number(value);
}

QString equipmentEffectText(EquipmentType equipmentType)
{//生成装备效果说明文本
    const EquipmentInfo& info = equipmentInfo(equipmentType);//装备配置
    QStringList effects;//装备效果文本列表

    if (info.atkBonus != 0) {
        effects.append(QString::fromUtf8("ATK %1").arg(signedNumberText(info.atkBonus)));//攻击力变化
    }
    if (info.maxHpBonus != 0) {
        effects.append(QString::fromUtf8("HP %1").arg(signedNumberText(info.maxHpBonus)));//生命值变化
    }
    if (info.maxManaBonus != 0) {
        effects.append(QString::fromUtf8("Max Mana %1").arg(signedNumberText(info.maxManaBonus)));//最大法力变化
    }
    if (info.attackSpeedPercentBonus != 0) {
        effects.append(QString::fromUtf8("攻击速度 +%1%").arg(info.attackSpeedPercentBonus));//攻击速度变化
    }
    if (info.attackIntervalReductionMs != 0) {
        effects.append(QString::fromUtf8("攻击间隔 -%1ms").arg(info.attackIntervalReductionMs));//攻击间隔变化
    }

    return effects.isEmpty() ? QString::fromUtf8("无属性变化") : effects.join(QString::fromUtf8("，"));//拼接最终说明
}

} // namespace

GameWindow::GameWindow(QWidget* parent)
    // 构造主窗口，并创建所有 GUI 控件和游戏对象。
    : QMainWindow(parent)
    , m_centralWidget(new QWidget(this))
    , m_mainLayout(new QVBoxLayout())
    , m_view(new QGraphicsView(this))
    , m_hpLabel(new QLabel(this))
    , m_goldLabel(new QLabel(this))
    , m_populationLabel(new QLabel(this))
    , m_stageLabel(new QLabel(this))
    , m_phaseLabel(new QLabel(this))
    , m_prepTimeLabel(new QLabel(this))
    , m_resultLabel(new QLabel(this))
    , m_resetButton(new QPushButton(QString::fromUtf8("重置"), this))
    , m_advancePhaseButton(new QPushButton(this))
    , m_saveButton(new QPushButton(QString::fromUtf8("存档"), this))
    , m_loadButton(new QPushButton(QString::fromUtf8("读档"), this))
    , m_guideButton(new QPushButton(QString::fromUtf8("游玩攻略"), this))
    , m_refreshShopButton(new QPushButton(QString::fromUtf8("刷新商店"), this))
    , m_upgradePopulationButton(new QPushButton(QString::fromUtf8("升级人口"), this))
    , m_starUpgradeButton(new QPushButton(QString::fromUtf8("升星检查"), this))
    , m_unequipButton(new QPushButton(QString::fromUtf8("卸下装备"), this))
    , m_synergyLabel(new QLabel(this))
    , m_combatTimer(new QTimer(this))
    , m_selectedBenchIndex(-1)
    , m_dragBenchIndex(-1)
    , m_benchPressPos(-1, -1)
    , m_selectedEquipmentIndex(-1)
    , m_dragEquipmentIndex(-1)
    , m_equipmentPressPos(-1, -1)
    , m_unequipMode(false)
    , m_selectedBoardUnitId(-1)
    , m_gameOverNoticeShown(false)
    , m_game(new Game(this))
{
    setupUI();//搭建界面布局
    m_game->initialize();//初始化游戏逻辑
    connectUnitItemDrops();//连接单位拖拽到备战区的信号
    refreshPanels();//刷新 HUD、Bench、商店、装备、羁绊
    updateCombatTimer();//根据当前阶段启动或停止战斗计时器
}

GameWindow::~GameWindow() = default;

void GameWindow::onResetButtonClicked()
{//点击重置按钮后恢复新一局状态
    if (m_game) {
        m_game->reset();//重置核心游戏状态
        connectUnitItemDrops();//场景重建后重新连接单位拖拽信号
        m_selectedBenchIndex = -1;//清空选中的备战区槽位
        m_selectedEquipmentIndex = -1;//清空选中的装备槽位
        m_unequipMode = false;//退出卸装模式
        m_selectedBoardUnitId = -1;//清空选中的棋盘单位
        m_gameOverNoticeShown = false;//允许重新弹出游戏结束提示
        refreshPanels();//刷新全部面板
        updateCombatTimer();//更新战斗计时器
    }
}

void GameWindow::onAdvancePhaseButtonClicked()
{//点击主阶段按钮后推进游戏阶段
    if (m_game) {
        const GamePhase previousPhase = m_game->phase();//记录推进前阶段，用于判断是否刚结束结算
        const int previousGold = m_game->player().gold();//记录推进前金币，用于结算提示
        const int previousHp = m_game->player().hp();//记录推进前生命，用于结算提示

        m_game->advancePhase();//核心逻辑推进阶段
        connectUnitItemDrops();//阶段变化可能重建场景，重新连接拖拽信号
        m_selectedBenchIndex = -1;//阶段切换后取消备战区选择
        m_selectedEquipmentIndex = -1;//阶段切换后取消装备选择
        m_unequipMode = false;//阶段切换后退出卸装模式
        m_selectedBoardUnitId = -1;//阶段切换后清空棋盘单位选择
        refreshPanels();//刷新全部面板
        updateCombatTimer();//战斗阶段启动计时器，其他阶段停止

        if (previousPhase == GamePhase::Resolve) {
            showPostResolveNotice(previousGold, previousHp);//从结算进入下一回合时显示奖励/扣血结果
        }
        showGameOverNoticeIfNeeded();//若玩家死亡则提示游戏结束
    }
}

void GameWindow::onCombatTick()
{//战斗计时器每 50ms 触发一次
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    if (m_game->phase() != GamePhase::Combat) {
        updateCombatTimer();//非战斗阶段应停止计时器
        refreshPanels();//刷新界面状态
        return;//不推进战斗
    }

    m_game->updateCombat(50);//推进 50ms 战斗逻辑
    if (m_view->scene()) {
        m_view->scene()->update();//刷新棋盘绘制
    }

    refreshPanels();//同步 HUD 和各面板
    updateCombatTimer();//若战斗结束则停止计时器
    showGameOverNoticeIfNeeded();//检查是否需要弹出失败提示
}

void GameWindow::onSaveButtonClicked()
{//点击存档按钮后选择路径并保存游戏
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8("存档"),
        QString(),
        QString::fromUtf8("Synera 存档 (*.json);;所有文件 (*.*)"));//玩家选择的存档路径

    if (path.isEmpty()) {
        return;//用户取消选择时直接返回
    }

    if (m_game->saveToFile(path)) {
        QMessageBox::information(this,
                                 QString::fromUtf8("存档成功"),
                                 QString::fromUtf8("存档已保存。"));
    } else {
        QMessageBox::warning(this,
                             QString::fromUtf8("存档失败"),
                             QString::fromUtf8("无法写入该存档路径。"));
    }
}

void GameWindow::onLoadButtonClicked()
{//点击读档按钮后选择文件并恢复游戏
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    const QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("读取存档"),
        QString(),
        QString::fromUtf8("Synera 存档 (*.json);;所有文件 (*.*)"));//玩家选择的读档路径

    if (path.isEmpty()) {
        return;//用户取消选择时直接返回
    }

    if (!m_game->loadFromFile(path)) {
        QMessageBox::warning(this,
                             QString::fromUtf8("读档失败"),
                             QString::fromUtf8("无法读取该存档。"));
        return;//读档失败时不继续刷新
    }

    m_selectedBenchIndex = -1;//读档后清空备战区选择
    m_selectedEquipmentIndex = -1;//读档后清空装备选择
    m_unequipMode = false;//读档后退出卸装模式
    m_selectedBoardUnitId = -1;//读档后清空棋盘单位选择
    m_gameOverNoticeShown = false;//读档后允许重新提示游戏结束
    m_view->setScene(m_game->scene());//读档可能重建场景，需要重新挂载
    connectUnitItemDrops();//重新连接新场景中的单位拖拽信号
    refreshPanels();//刷新所有面板
    updateCombatTimer();//如果读入战斗阶段，则继续自动战斗

    QMessageBox::information(this,
                             QString::fromUtf8("读档成功"),
                             QString::fromUtf8("游戏状态已恢复。"));
}

bool GameWindow::eventFilter(QObject* watched, QEvent* event)
{//统一处理 Bench、装备和棋盘点击/拖拽事件
    for (int i = 0; i < m_benchButtons.size(); ++i) {
        if (watched != m_benchButtons.at(i)) {
            continue;//当前事件不是这个备战区按钮，检查下一个
        }

        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);//鼠标按下事件
            if (mouseEvent->button() == Qt::LeftButton
                && m_game
                && m_game->phase() == GamePhase::Prep
                && m_game->bench().hasUnitAt(i)) {
                m_dragBenchIndex = i;//记录正在拖拽的备战区槽位
                m_benchPressPos = mouseEvent->globalPosition().toPoint();//记录按下位置
            }
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragBenchIndex == i) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);//鼠标释放事件
            const QPoint viewportPos = m_view->viewport()->mapFromGlobal(mouseEvent->globalPosition().toPoint());//释放点转换到棋盘视图坐标
            if (m_view->viewport()->rect().contains(viewportPos)) {
                const QPoint boardPos = sceneToGrid(m_view->mapToScene(viewportPos));//释放点对应的棋盘格
                const bool moved = moveBenchAtBoardPosition(m_dragBenchIndex, boardPos);//尝试把 Bench 单位部署到棋盘
                m_dragBenchIndex = -1;//清空拖拽槽位
                m_benchPressPos = QPoint(-1, -1);//清空按下位置
                if (moved) {
                    return true;//部署成功，事件已处理
                }
            }

            m_dragBenchIndex = -1;//拖拽失败或未落在棋盘上，清空状态
            m_benchPressPos = QPoint(-1, -1);//清空按下位置
        }
    }

    for (int i = 0; i < m_equipmentButtons.size(); ++i) {
        if (watched != m_equipmentButtons.at(i)) {
            continue;//当前事件不是这个装备按钮
        }

        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);//鼠标按下事件
            if (mouseEvent->button() == Qt::LeftButton
                && m_game
                && m_game->phase() == GamePhase::Prep
                && i < m_game->playerEquipmentInventory().size()) {
                m_dragEquipmentIndex = i;//记录正在拖拽的装备下标
                m_equipmentPressPos = mouseEvent->globalPosition().toPoint();//记录按下位置
            }
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragEquipmentIndex == i) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);//鼠标释放事件
            const QPoint viewportPos = m_view->viewport()->mapFromGlobal(mouseEvent->globalPosition().toPoint());//释放点转换到视图坐标
            if (m_view->viewport()->rect().contains(viewportPos)) {
                const QPoint boardPos = sceneToGrid(m_view->mapToScene(viewportPos));//释放点对应棋盘格
                const bool equipped = equipInventoryAtBoardPosition(m_dragEquipmentIndex, boardPos, true);//尝试给该格单位穿装备
                m_dragEquipmentIndex = -1;//清空拖拽装备
                m_equipmentPressPos = QPoint(-1, -1);//清空按下位置
                if (equipped) {
                    return true;//穿戴成功，事件已处理
                }
            }

            m_dragEquipmentIndex = -1;//穿戴失败或未落在棋盘上，清空状态
            m_equipmentPressPos = QPoint(-1, -1);//清空按下位置
        }
    }

    if (watched == m_view->viewport()
        && event->type() == QEvent::MouseButtonPress
        && m_game
        && m_game->phase() == GamePhase::Prep) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);//棋盘鼠标按下事件
        if (mouseEvent->button() == Qt::LeftButton) {
            const QPointF scenePos = m_view->mapToScene(mouseEvent->pos());//视图坐标转场景坐标
            const QPoint boardPos = sceneToGrid(scenePos);//场景坐标转棋盘格

            if (m_selectedBenchIndex != -1 && moveBenchAtBoardPosition(m_selectedBenchIndex, boardPos)) {
                m_selectedBenchIndex = -1;//点击部署成功后取消 Bench 选择
                return true;//事件已处理
            }

            if (m_selectedEquipmentIndex != -1) {
                if (equipInventoryAtBoardPosition(m_selectedEquipmentIndex, boardPos, true)) {
                    m_selectedEquipmentIndex = -1;//点击穿戴成功后取消装备选择
                    return true;//事件已处理
                }
            }

            if (m_unequipMode) {
                selectBoardUnitAt(boardPos);//先选择点击到的棋盘单位
                if (m_selectedBoardUnitId != -1 && m_game->unequipUnitToInventory(m_selectedBoardUnitId)) {
                    m_unequipMode = false;//卸装成功后退出卸装模式
                    m_selectedBoardUnitId = -1;//清空单位选择
                    m_view->scene()->update();//刷新棋盘装备角标
                    refreshPanels();//刷新装备栏
                    return true;//事件已处理
                }
                if (m_selectedBoardUnitId != -1) {
                    QMessageBox::information(this,
                                             QString::fromUtf8("卸下装备"),
                                             QString::fromUtf8("这个英雄目前没有装备可以卸下。"));
                }
                m_unequipMode = false;//无论成功与否，本次卸装操作结束
                m_selectedBoardUnitId = -1;//清空单位选择
                refreshPanels();//刷新按钮状态
                return true;//事件已处理
            }

            if (m_selectedBenchIndex != -1 || m_selectedEquipmentIndex != -1 || m_unequipMode) {
                return true;//处于操作模式但未成功落点时，仍吞掉事件避免误选单位
            }

            selectBoardUnitAt(boardPos);//普通点击棋盘时选择单位
        }
    }

    return QMainWindow::eventFilter(watched, event);//其他事件交给父类处理
}

void GameWindow::setupUI()
{//创建和排列主窗口中的所有控件
    setCentralWidget(m_centralWidget);//设置中心控件
    m_centralWidget->setLayout(m_mainLayout);//主布局使用垂直布局

    setStyleSheet(R"(
        QMainWindow {
            background-color: #1d1329;
        }
        QWidget {
            background-color: #1d1329;
            color: #f7edc5;
            font-size: 13px;
        }
        QPushButton {
            background-color: #2b1b3d;
            color: #f7edc5;
            border: 1px solid #8f7437;
            border-radius: 4px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #392452;
            border-color: #d8b85d;
        }
        QPushButton:pressed {
            background-color: #160d22;
        }
        QPushButton:disabled {
            background-color: #21172d;
            color: #8f8498;
            border-color: #493858;
        }
        QLabel {
            color: #f7edc5;
        }
    )");

    m_view->setRenderHint(QPainter::Antialiasing, true);//棋盘绘制抗锯齿
    m_view->setDragMode(QGraphicsView::NoDrag);//拖拽逻辑由 UnitItem/GameWindow 自己处理
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);//隐藏横向滚动条
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);//隐藏纵向滚动条
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);//缩放锚点设为鼠标位置
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);//窗口变化时保持视图中心
    m_view->setMouseTracking(true);//允许无按键时也追踪鼠标
    m_view->viewport()->setMouseTracking(true);//视口也开启鼠标追踪
    m_view->viewport()->installEventFilter(this);//拦截棋盘点击，用于部署/穿装/卸装

    QWidget* hudBar = new QWidget(this);
    QHBoxLayout* hudLayout = new QHBoxLayout(hudBar);
    hudLayout->setContentsMargins(0, 0, 0, 0);
    hudLayout->setSpacing(18);
    hudLayout->addWidget(m_hpLabel);
    hudLayout->addWidget(m_goldLabel);
    hudLayout->addWidget(m_populationLabel);
    hudLayout->addWidget(m_stageLabel);
    hudLayout->addWidget(m_phaseLabel);
    hudLayout->addWidget(m_prepTimeLabel);
    hudLayout->addStretch();
    hudLayout->addWidget(m_guideButton);
    m_mainLayout->addWidget(hudBar);

    m_resultLabel->setWordWrap(true);
    m_resultLabel->setMinimumHeight(30);
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setStyleSheet(QStringLiteral(
        "background-color: #2b1b3d;"
        "border: 1px solid #8f7437;"
        "border-radius: 4px;"
        "padding: 6px 10px;"
        "color: #f7edc5;"
        "font-weight: bold;"));
    m_mainLayout->addWidget(m_resultLabel);

    m_mainLayout->addWidget(m_view, 1);

    QWidget* benchBar = new QWidget(this);//备战区横向容器
    QHBoxLayout* benchLayout = new QHBoxLayout(benchBar);//备战区布局
    benchLayout->setContentsMargins(0, 0, 0, 0);
    benchLayout->setSpacing(6);
    benchLayout->addWidget(new QLabel(QString::fromUtf8("备战区"), this));

    for (int i = 0; i < 8; ++i) {
        QPushButton* button = new QPushButton(this);//单个备战区槽位按钮
        button->setMinimumWidth(86);
        button->setMinimumHeight(34);
        button->installEventFilter(this);
        m_benchButtons.append(button);
        benchLayout->addWidget(button);

        connect(button, &QPushButton::clicked, this, [this, i]() {
            selectBenchSlot(i);//点击槽位时进入/取消 Bench 选择
        });
    }

    benchLayout->addStretch();
    m_mainLayout->addWidget(benchBar);

    QWidget* shopBar = new QWidget(this);//商店横向容器
    QHBoxLayout* shopLayout = new QHBoxLayout(shopBar);//商店布局
    shopLayout->setContentsMargins(0, 0, 0, 0);
    shopLayout->setSpacing(6);
    shopLayout->addWidget(new QLabel(QString::fromUtf8("商店"), this));

    for (int i = 0; i < Shop::SLOT_COUNT; ++i) {
        QPushButton* button = new QPushButton(this);//单个商店招募位按钮
        button->setMinimumWidth(106);
        button->setMinimumHeight(46);
        m_shopButtons.append(button);
        shopLayout->addWidget(button);

        connect(button, &QPushButton::clicked, this, [this, i]() {
            if (m_game && m_game->purchaseShopUnit(i)) {
                connectUnitItemDrops();//购买可能触发场景重建，需要重连拖拽信号
                m_selectedBenchIndex = -1;//购买后清空 Bench 选择
                m_selectedEquipmentIndex = -1;//购买后清空装备选择
                m_unequipMode = false;//购买后退出卸装模式
                m_selectedBoardUnitId = -1;//购买后清空棋盘单位选择
                refreshPanels();//刷新商店、Bench、HUD
            }
        });
    }

    shopLayout->addWidget(m_refreshShopButton);
    shopLayout->addStretch();
    m_mainLayout->addWidget(shopBar);

    QWidget* equipmentBar = new QWidget(this);//装备栏横向容器
    QHBoxLayout* equipmentLayout = new QHBoxLayout(equipmentBar);//装备栏布局
    equipmentLayout->setContentsMargins(0, 0, 0, 0);
    equipmentLayout->setSpacing(6);
    equipmentLayout->addWidget(new QLabel(QString::fromUtf8("装备"), this));

    for (int i = 0; i < 10; ++i) {
        QPushButton* button = new QPushButton(this);//单个装备库存按钮
        button->setMinimumWidth(76);
        button->setMinimumHeight(32);
        button->installEventFilter(this);
        m_equipmentButtons.append(button);
        equipmentLayout->addWidget(button);

        connect(button, &QPushButton::clicked, this, [this, i]() {
            selectEquipmentSlot(i);//点击装备后进入穿戴选择
        });
    }

    equipmentLayout->addWidget(m_unequipButton);
    equipmentLayout->addStretch();
    m_mainLayout->addWidget(equipmentBar);

    m_synergyLabel->setWordWrap(true);
    m_synergyLabel->setMinimumHeight(28);
    m_mainLayout->addWidget(m_synergyLabel);

    QWidget* controlBar = new QWidget(this);//底部控制按钮容器
    QHBoxLayout* controlLayout = new QHBoxLayout(controlBar);//控制按钮布局
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addWidget(m_advancePhaseButton);
    controlLayout->addWidget(m_upgradePopulationButton);
    controlLayout->addWidget(m_starUpgradeButton);
    controlLayout->addWidget(m_saveButton);
    controlLayout->addWidget(m_loadButton);
    controlLayout->addWidget(m_resetButton);
    controlLayout->addStretch();
    m_mainLayout->addWidget(controlBar);

    connect(m_resetButton, &QPushButton::clicked,
            this, &GameWindow::onResetButtonClicked);
    connect(m_advancePhaseButton, &QPushButton::clicked,
            this, &GameWindow::onAdvancePhaseButtonClicked);
    connect(m_saveButton, &QPushButton::clicked,
            this, &GameWindow::onSaveButtonClicked);
    connect(m_loadButton, &QPushButton::clicked,
            this, &GameWindow::onLoadButtonClicked);
    connect(m_refreshShopButton, &QPushButton::clicked,
            this, &GameWindow::onRefreshShopButtonClicked);
    connect(m_upgradePopulationButton, &QPushButton::clicked,
            this, &GameWindow::onUpgradePopulationButtonClicked);
    connect(m_starUpgradeButton, &QPushButton::clicked,
            this, &GameWindow::onStarUpgradeButtonClicked);
    connect(m_guideButton, &QPushButton::clicked,
            this, &GameWindow::onGuideButtonClicked);
    connect(m_unequipButton, &QPushButton::clicked,
            this, &GameWindow::selectUnequipMode);
    connect(m_combatTimer, &QTimer::timeout,
            this, &GameWindow::onCombatTick);

    m_combatTimer->setInterval(50);//战斗阶段每 50ms 更新一次

    m_view->setScene(m_game->scene());//把游戏场景挂到视图上
}

void GameWindow::onRefreshShopButtonClicked()
{//点击刷新商店按钮
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    if (m_game->refreshShop()) {
        connectUnitItemDrops();//刷新商店可能重建场景，重新连接单位拖拽信号
        m_selectedBenchIndex = -1;//清空 Bench 选择
        m_selectedEquipmentIndex = -1;//清空装备选择
        m_unequipMode = false;//退出卸装模式
        m_selectedBoardUnitId = -1;//清空棋盘单位选择
        refreshPanels();//刷新界面
        updateCombatTimer();//同步计时器状态
    }
}

void GameWindow::onUpgradePopulationButtonClicked()
{//点击升级人口按钮
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    if (!m_game->upgradePopulation()) {
        QMessageBox::information(this,
                                 QString::fromUtf8("升级失败"),
                                 QString::fromUtf8("只有准备阶段可以升级人口，且需要 %1 金币。")
                                     .arg(Player::POPUPGRADE_COST));
        return;//升级失败后不继续刷新
    }

    refreshPanels();//升级成功后刷新人口和金币
}

void GameWindow::onStarUpgradeButtonClicked()
{//点击升星检查按钮
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    const bool upgraded = m_game->processPlayerStarUpgrades();//执行一次手动升星检查
    refreshPanels();//刷新星级、Bench、棋盘和羁绊

    QMessageBox::information(this,
                             QString::fromUtf8("升星检查"),
                             upgraded
                                 ? QString::fromUtf8("已完成可用的 3 合 1 升星。")
                                 : QString::fromUtf8("当前没有 3 个同名同星英雄可升星。"));
}

void GameWindow::onGuideButtonClicked()
{//打开可滚动的游玩攻略窗口
    QDialog dialog(this);//攻略弹窗
    dialog.setWindowTitle(QString::fromUtf8("游玩攻略"));
    dialog.resize(620, 560);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);//弹窗布局
    QTextEdit* guideText = new QTextEdit(&dialog);//可滚动攻略文本框
    guideText->setReadOnly(true);
    guideText->setText(QString::fromUtf8(
            "基础流程\n"
            "1. 准备阶段：购买英雄、部署到下半场、穿戴装备、升级人口。\n"
            "2. 点击“开始战斗”后自动战斗，战斗中不能拖拽。\n"
            "3. 结算后进入下一回合。当前是无限关卡模式，敌人随关卡增强。\n\n"
            "英雄\n"
            "枪手：HP 220，ATK 20，Range 3，Max Mana 60。每第 4 次普攻双倍伤害；满蓝对最近敌人造成 ATK*3。\n"
            "治疗师：HP 240，ATK 10，Range 2，Max Mana 60。每移动 2 步治疗左右友军 3；满蓝全体友军回血 25。\n"
            "战神：HP 500，ATK 50，Range 1，Max Mana 80。不可升星；每移动 2 步治疗左右友军 5；满蓝对目标及上下左右造成范围伤害。\n"
            "殉道者：HP 260，ATK 20，Range 1。每移动 2 步治疗左右友军 3；死亡后治疗友军。\n"
            "模仿者：HP 220，ATK 15，Range 2。尝试模仿右侧单位上一次动作。\n\n"
            "法力与主动技能\n"
            "普攻会回复法力值；蓝条满时单位进入施法状态，自动释放自己的主动技能，然后法力清零。\n"
            "枪手、治疗师、战神拥有不同的满蓝技能：枪手爆发单体伤害，治疗师全体回血，战神范围伤害。\n\n"
            "羁绊\n"
            "枪手 2/4：场上有 2 个枪手时全体我方 ATK +10；场上有 4 个枪手时提升为 +20。\n"
            "治疗师 2：战斗开始前全体我方恢复 25 HP。\n"
            "殉道者 2：殉道者死亡治疗 +5。\n"
            "模仿者 2：模仿者攻击前尝试模仿右侧动作，并 ATK +10。\n"
            "战意 3：进攻/斗志型羁绊。拥有“战意”的我方上场英雄达到 3 个时激活，全体我方攻击间隔 -100ms。\n"
            "拥有战意的英雄：枪手、战神、殉道者。\n\n"
            "装备\n"
            "铁剑：ATK +15。\n"
            "锁子甲：Max HP +150。\n"
            "迅捷手套：攻击速度 +20%。\n"
            "蓝晶石：Max Mana -30。\n\n"
            "操作提示\n"
            "备战区：可以把备战区英雄拖到我方半场部署；拖到已有我方单位上可以交换。\n"
            "棋盘：准备阶段可以拖拽我方英雄重新排兵布阵。\n"
            "装备：可以点击装备后再点击我方英雄穿戴，也可以把装备拖到我方英雄身上穿戴。\n"
            "卸装：先点击一个我方英雄，再点击“卸下装备”。没有装备会提示。\n"
            "升星：3 个同名同星英雄自动或点击“升星检查”合成；战神不可升星。\n"
            "升星效果：合成后的英雄星级 +1，HP 和 ATK 提升，并保留最近获得的那个英雄位置；被合成材料身上的装备会回到装备栏。\n"
            "人口：点击“升级人口”花费金币提高上阵上限。\n\n"
            "连胜 / 连败经济\n"
            "连续胜利会累积连胜，连续失败会累积连败。\n"
            "2 连额外 +1 金币，3-4 连额外 +2 金币，5 连及以上额外 +3 金币。\n\n"
            "敌人说明\n"
            "敌人不使用升星系统，敌方强度按关卡进行数值成长。"));
    guideText->setStyleSheet(QString::fromUtf8(
        "QTextEdit {"
        "background:#2b1745;"
        "color:#fff4c7;"
        "border:2px solid #d9aa3d;"
        "border-radius:8px;"
        "padding:10px;"
        "font-size:14px;"
        "}"
    ));

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);//确认按钮区域
    buttons->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("知道了"));//按钮中文文本
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    layout->addWidget(guideText);
    layout->addWidget(buttons);
    dialog.exec();//以模态窗口显示攻略
}

void GameWindow::refreshHud()
{//刷新顶部 HUD 和主要按钮状态
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    m_hpLabel->setText(QString::fromUtf8("生命: %1").arg(m_game->player().hp()));
    QString streakText;//当前连续胜负状态说明
    if (m_game->player().winStreak() > 0) {
        streakText = QString::fromUtf8("（连胜 %1）").arg(m_game->player().winStreak());
    } else if (m_game->player().lossStreak() > 0) {
        streakText = QString::fromUtf8("（连败 %1）").arg(m_game->player().lossStreak());
    }
    m_goldLabel->setText(QString::fromUtf8("金币: %1%2")
                             .arg(m_game->player().gold())
                             .arg(streakText));
    m_populationLabel->setText(QString::fromUtf8("人口: %1/%2")
                                   .arg(m_game->deployedUnitCount())
                                   .arg(m_game->player().populationCap()));
    m_stageLabel->setText(QString::fromUtf8("关卡: %1").arg(m_game->player().currentStage()));
    m_phaseLabel->setText(QString::fromUtf8("阶段: %1").arg(phaseText(m_game->phase())));
    m_prepTimeLabel->setText(QString::fromUtf8("准备时间: 无限"));
    m_advancePhaseButton->setText(advanceButtonText(m_game->phase()));
    m_advancePhaseButton->setEnabled(!m_game->isGameOver());
    m_upgradePopulationButton->setText(QString::fromUtf8("升级人口 -%1").arg(Player::POPUPGRADE_COST));
    m_upgradePopulationButton->setEnabled(m_game->phase() == GamePhase::Prep && !m_game->isGameOver());
    m_starUpgradeButton->setEnabled(m_game->phase() == GamePhase::Prep && !m_game->isGameOver());
}

void GameWindow::refreshPanels()
{//统一刷新所有 GUI 面板
    refreshHud();//刷新生命、金币、人口、关卡、阶段
    refreshBench();//刷新备战区
    refreshShop();//刷新商店
    refreshEquipment();//刷新装备栏
    refreshSynergies();//刷新羁绊栏
    refreshResultNotice();//刷新结算提示
}

void GameWindow::connectUnitItemDrops()
{//给场景中的 UnitItem 连接“拖到备战区”的信号
    if (!m_game || !m_game->scene()) {
        return;//没有场景时不能连接
    }

    const QList<QGraphicsItem*> items = m_game->scene()->items();//当前场景中的所有图元
    for (QGraphicsItem* item : items) {
        UnitItem* unitItem = dynamic_cast<UnitItem*>(item);//尝试转换为单位图元
        if (!unitItem) {
            continue;//不是单位图元就跳过
        }

        if (unitItem->property("benchDropConnected").toBool()) {
            continue;//已经连接过则跳过
        }

        connect(unitItem,
                &UnitItem::dragDroppedWithScreenPos,
                this,
                [this](int, const QPoint& sourceGrid, const QPointF&, const QPoint& screenPos) {
                    moveBoardUnitToBenchAtScreenPosition(sourceGrid, screenPos);//根据屏幕位置判断是否拖到 Bench
                });
        unitItem->setProperty("benchDropConnected", true);//用属性标记已连接
    }
}

void GameWindow::refreshBench()
{//刷新备战区按钮文字和选中状态
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    for (int i = 0; i < m_benchButtons.size(); ++i) {
        QPushButton* button = m_benchButtons.at(i);//当前槽位按钮
        Unit* unit = m_game->bench().getUnitAt(i);//当前槽位单位

        if (unit) {
            button->setText(QString::fromUtf8("%1\n%2★")
                                .arg(unit->name())
                                .arg(unit->star()));
            button->setEnabled(m_game->phase() == GamePhase::Prep);//只有准备阶段允许操作
        } else {
            button->setText(QString::fromUtf8("空"));
            button->setEnabled(false);//空槽位不可点击选择
        }

        if (i == m_selectedBenchIndex) {
            button->setStyleSheet(QStringLiteral(
                "border: 2px solid #d8b85d; background-color: #4a315f;"));
        } else if (i == m_dragBenchIndex) {
            button->setStyleSheet(QStringLiteral(
                "border: 2px dashed #d8b85d; background-color: #3a254f;"));
        } else {
            button->setStyleSheet(QString());
        }
    }
}

void GameWindow::selectBenchSlot(int index)
{//点击选择或取消选择某个备战区槽位
    if (!m_game || m_game->phase() != GamePhase::Prep || !m_game->bench().hasUnitAt(index)) {
        return;//只能在准备阶段选择有单位的槽位
    }

    m_selectedBenchIndex = (m_selectedBenchIndex == index) ? -1 : index;//再次点击同一槽位则取消选择
    m_selectedEquipmentIndex = -1;//选择 Bench 时取消装备选择
    m_unequipMode = false;//退出卸装模式
    m_selectedBoardUnitId = -1;//清空棋盘单位选择
    refreshBench();//刷新 Bench 高亮
    refreshEquipment();//刷新装备按钮状态
}

void GameWindow::refreshShop()
{//刷新商店招募位
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    for (int i = 0; i < m_shopButtons.size(); ++i) {
        QPushButton* button = m_shopButtons.at(i);//当前商店按钮
        Unit* unit = m_game->shop().getUnitAt(i);//当前商店槽位单位

        if (unit) {
            button->setText(QString::fromUtf8("%1\n%2 金币")
                                .arg(unit->name())
                                .arg(Shop::PURCHASE_COST));
            button->setEnabled(m_game->phase() == GamePhase::Prep);//只有准备阶段可以买
        } else {
            button->setText(QString::fromUtf8("已购买"));
            button->setEnabled(false);//空商店位不可购买
        }
    }

    m_refreshShopButton->setText(QString::fromUtf8("刷新商店 -%1").arg(Shop::REFRESH_COST));//显示刷新费用
    m_refreshShopButton->setEnabled(m_game->phase() == GamePhase::Prep);//只有准备阶段可刷新
}

void GameWindow::refreshEquipment()
{//刷新装备栏按钮
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    const QList<EquipmentType> inventory = m_game->playerEquipmentInventory();//当前玩家装备库存
    for (int i = 0; i < m_equipmentButtons.size(); ++i) {
        QPushButton* button = m_equipmentButtons.at(i);//当前装备按钮

        if (i < inventory.size()) {
            button->setText(equipmentName(inventory.at(i)));//显示装备名称
            button->setEnabled(m_game->phase() == GamePhase::Prep);//准备阶段才能穿装备
        } else {
            button->setText(QString::fromUtf8("空"));
            button->setEnabled(false);//空装备槽不可点击
        }

        if (i == m_selectedEquipmentIndex) {
            button->setStyleSheet(QStringLiteral(
                "border: 2px solid #d8b85d; background-color: #4a315f;"));
        } else if (i == m_dragEquipmentIndex) {
            button->setStyleSheet(QStringLiteral(
                "border: 2px dashed #d8b85d; background-color: #3a254f;"));
        } else {
            button->setStyleSheet(QString());
        }
    }

    m_unequipButton->setEnabled(m_game->phase() == GamePhase::Prep);//只有准备阶段可卸装
    if (m_unequipMode) {
        m_unequipButton->setStyleSheet(QStringLiteral(
            "border: 2px solid #d8b85d; background-color: #4a315f;"));
    } else {
        m_unequipButton->setStyleSheet(QString());
    }
}

void GameWindow::refreshSynergies()
{//刷新当前激活羁绊说明
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    const QStringList descriptions = SynergySystem::activeSynergyDescriptions(m_game->board());//当前激活羁绊描述
    if (descriptions.isEmpty()) {
        m_synergyLabel->setText(QString::fromUtf8("羁绊: 无激活"));
        return;//没有激活羁绊时显示默认文本
    }

    m_synergyLabel->setText(QString::fromUtf8("羁绊: %1").arg(descriptions.join(QString::fromUtf8(" | "))));//多个羁绊用分隔符显示
}

void GameWindow::refreshResultNotice()
{//刷新顶部结算/战斗提示
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    if (m_game->isGameOver()) {
        if (m_game->result() == GameResult::Defeat) {
            m_resultLabel->setText(QString::fromUtf8("游戏结束：生命归零，挑战失败"));
        } else {
            m_resultLabel->setText(QString::fromUtf8("游戏结束：挑战胜利"));
        }
        m_resultLabel->setVisible(true);
        return;//游戏结束提示优先级最高
    }

    if (m_game->phase() == GamePhase::Resolve) {
        const bool hasAliveEnemy = hasAliveUnitForOwner(m_game->board(), Unit::Owner::EnemyCtrl);//敌方是否还有存活单位
        const bool hasAlivePlayer = hasAliveUnitForOwner(m_game->board(), Unit::Owner::PlayerCtrl);//我方是否还有存活单位
        if (!hasAliveEnemy && hasAlivePlayer) {
            m_resultLabel->setText(QString::fromUtf8("本轮胜利！点击“下一回合”领取金币，并有机会获得装备。"));
        } else {
            m_resultLabel->setText(QString::fromUtf8("本轮失败。点击“下一回合”结算并扣除生命。"));
        }
        m_resultLabel->setVisible(true);
        return;//结算阶段显示本轮结果
    }

    if (m_game->phase() == GamePhase::Combat) {
        m_resultLabel->setText(QString::fromUtf8("战斗进行中！"));
        m_resultLabel->setVisible(true);
        return;//战斗阶段显示战斗进行中
    }

    m_resultLabel->clear();//准备阶段无结算提示
    m_resultLabel->setVisible(false);//隐藏提示条
}

void GameWindow::updateCombatTimer()
{//根据当前阶段启动或停止自动战斗计时器
    if (!m_game || m_game->phase() != GamePhase::Combat || m_game->isGameOver()) {
        if (m_combatTimer->isActive()) {
            m_combatTimer->stop();//非战斗阶段或游戏结束时停止 tick
        }
        return;//不需要启动计时器
    }

    if (!m_combatTimer->isActive()) {
        m_combatTimer->start();//进入战斗阶段后启动自动 tick
    }
}

bool GameWindow::moveBenchAtBoardPosition(int benchIndex, const QPoint& boardPos)
{//把备战区单位移动到棋盘指定格
    if (!m_game || m_game->phase() != GamePhase::Prep) {
        return false;//只能在准备阶段部署
    }

    if (!m_game->moveBenchUnitToBoard(benchIndex, boardPos)) {
        return false;//核心规则拒绝部署时返回失败
    }

    m_selectedBenchIndex = -1;//部署后清空 Bench 选择
    m_selectedEquipmentIndex = -1;//清空装备选择
    m_unequipMode = false;//退出卸装模式
    m_selectedBoardUnitId = -1;//清空棋盘单位选择
    if (m_view->scene()) {
        m_view->scene()->update();//刷新棋盘显示
    }
    refreshPanels();//刷新全部面板
    return true;//部署成功
}

bool GameWindow::moveBoardUnitToBenchAtScreenPosition(const QPoint& boardPos, const QPoint& screenPos)
{//把棋盘上的我方单位拖回备战区空位
    if (!m_game || m_game->phase() != GamePhase::Prep) {
        return false;//只能在准备阶段拖回
    }

    Unit* unit = m_game->board().getUnitAt(boardPos);//源棋盘格上的单位
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        return false;//只能拖回我方单位
    }

    for (int i = 0; i < m_benchButtons.size(); ++i) {
        QPushButton* button = m_benchButtons.at(i);//当前备战区按钮
        const QPoint localPos = button->mapFromGlobal(screenPos);//屏幕坐标转按钮内部坐标
        if (!button->rect().contains(localPos) || m_game->bench().hasUnitAt(i)) {
            continue;//没有落在该按钮上，或该槽位已有单位
        }

        if (!m_game->moveBoardUnitToBench(boardPos, i)) {
            return false;//核心规则拒绝移动时返回失败
        }

        m_selectedBenchIndex = -1;//清空 Bench 选择
        m_selectedEquipmentIndex = -1;//清空装备选择
        m_selectedBoardUnitId = -1;//清空棋盘单位选择
        m_unequipMode = false;//退出卸装模式
        if (m_view->scene()) {
            m_view->scene()->update();//刷新棋盘显示
        }
        refreshPanels();//刷新全部面板
        return true;//拖回成功
    }

    return false;//没有落到可用备战区空位
}

void GameWindow::selectBoardUnitAt(const QPoint& boardPos)
{//选择棋盘上的我方单位
    m_selectedBoardUnitId = -1;//先清空旧选择
    if (!m_game || m_game->phase() != GamePhase::Prep) {
        return;//只能在准备阶段选择
    }

    Unit* unit = m_game->board().getUnitAt(boardPos);//点击格上的单位
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        return;//只能选择我方单位
    }

    m_selectedBoardUnitId = unit->id();//记录选中单位 id
    m_selectedBenchIndex = -1;//选择棋盘单位时取消 Bench 选择
    m_selectedEquipmentIndex = -1;//取消装备选择
    m_unequipMode = false;//退出卸装模式
    refreshBench();//刷新 Bench 高亮
    refreshEquipment();//刷新装备栏状态
}

bool GameWindow::equipInventoryAtBoardPosition(int inventoryIndex, const QPoint& boardPos, bool showNotice)
{//把装备栏中的装备穿到棋盘指定单位身上
    if (!m_game || m_game->phase() != GamePhase::Prep) {
        return false;//只能在准备阶段穿装备
    }

    const QList<EquipmentType> inventory = m_game->playerEquipmentInventory();//当前装备库存
    if (inventoryIndex < 0 || inventoryIndex >= inventory.size()) {
        return false;//装备下标非法
    }

    Unit* unit = m_game->board().getUnitAt(boardPos);//目标格上的单位
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        return false;//只能给我方单位穿装备
    }

    const EquipmentType equipmentType = inventory.at(inventoryIndex);//要穿戴的装备类型
    if (!m_game->equipUnitFromInventory(unit->id(), inventoryIndex)) {
        return false;//核心规则拒绝穿戴时返回失败
    }

    m_selectedBenchIndex = -1;//清空 Bench 选择
    m_selectedEquipmentIndex = -1;//清空装备选择
    m_unequipMode = false;//退出卸装模式
    m_selectedBoardUnitId = -1;//清空单位选择
    if (m_view->scene()) {
        m_view->scene()->update();//刷新单位装备角标
    }
    refreshPanels();//刷新装备栏和 HUD

    if (showNotice) {
        showEquipmentAppliedNotice(equipmentType, unit);//按需显示穿戴提示
    }

    return true;//穿戴成功
}

void GameWindow::showEquipmentAppliedNotice(EquipmentType equipmentType, Unit* unit)
{//显示装备穿戴成功提示
    if (!unit || equipmentType == EquipmentType::None) {
        return;//单位或装备无效时不提示
    }

    QMessageBox::information(this,
                             QString::fromUtf8("装备已穿戴"),
                             QString::fromUtf8("%1 穿戴了 %2\n加成：%3")
                                 .arg(unit->name())
                                 .arg(equipmentName(equipmentType))
                                 .arg(equipmentEffectText(equipmentType)));
}

void GameWindow::showPostResolveNotice(int previousGold, int previousHp)
{//从结算阶段进入下一回合后，显示金币/生命变化
    if (!m_game) {
        return;//没有游戏对象时不处理
    }

    const int goldDelta = m_game->player().gold() - previousGold;//本次结算金币变化
    const int hpDelta = m_game->player().hp() - previousHp;//本次结算生命变化

    if (hpDelta < 0) {
        QString message = QString::fromUtf8("本轮失败，生命 %1").arg(hpDelta);//失败扣血提示
        if (goldDelta > 0) {
            message += QString::fromUtf8("\n失败补给：金币 +%1").arg(goldDelta);//失败也给少量金币
        }
        if (m_game->player().lossStreak() >= 2) {
            message += QString::fromUtf8("\n连败 %1，已包含额外经济奖励。")
                           .arg(m_game->player().lossStreak());//提示连败奖励已计入金币变化
        }

        QMessageBox::warning(this,
                             QString::fromUtf8("结算"),
                             message);
        return;//失败结算提示结束
    }

    if (goldDelta > 0) {
        QString message = QString::fromUtf8("本轮胜利！金币 +%1").arg(goldDelta);//胜利金币提示
        if (m_game->player().winStreak() >= 2) {
            message += QString::fromUtf8("\n连胜 %1，已包含额外经济奖励。")
                           .arg(m_game->player().winStreak());//提示连胜奖励已计入金币变化
        }
        if (m_game->lastEquipmentDropped()) {
            message += QString::fromUtf8("\n获得装备：%1").arg(equipmentName(m_game->lastDroppedEquipment()));//显示掉落装备
        } else {
            message += QString::fromUtf8("\n本轮没有掉落装备");//没有掉落时也明确提示
        }

        QMessageBox::information(this,
                                 QString::fromUtf8("结算"),
                                 message);
        return;//胜利结算提示结束
    }
}

void GameWindow::showGameOverNoticeIfNeeded()
{//如果游戏结束且还没提示过，就弹出游戏结束提示
    if (!m_game || !m_game->isGameOver() || m_gameOverNoticeShown) {
        return;//未结束或已经提示过，不重复弹窗
    }

    m_gameOverNoticeShown = true;//记录已经提示，避免重复弹窗
    if (m_game->result() == GameResult::Defeat) {
        QMessageBox::critical(this,
                              QString::fromUtf8("游戏结束"),
                              QString::fromUtf8("玩家生命已归零，挑战失败。"));
    } else {
        QMessageBox::information(this,
                                 QString::fromUtf8("游戏结束"),
                                 QString::fromUtf8("挑战胜利！"));
    }
}

void GameWindow::selectEquipmentSlot(int index)
{//点击选择或取消选择装备栏中的装备
    if (!m_game || m_game->phase() != GamePhase::Prep || index < 0 || index >= m_game->playerEquipmentInventory().size()) {
        return;//只能在准备阶段选择有效装备
    }

    m_selectedEquipmentIndex = (m_selectedEquipmentIndex == index) ? -1 : index;//再次点击同一装备则取消选择
    m_selectedBenchIndex = -1;//选择装备时取消 Bench 选择
    m_unequipMode = false;//退出卸装模式
    m_selectedBoardUnitId = -1;//清空棋盘单位选择
    refreshBench();//刷新 Bench 高亮
    refreshEquipment();//刷新装备高亮
}

void GameWindow::selectUnequipMode()
{//点击卸下装备按钮
    if (!m_game || m_game->phase() != GamePhase::Prep) {
        return;//只能在准备阶段卸装
    }

    if (m_selectedBoardUnitId == -1) {
        QMessageBox::information(this,
                                 QString::fromUtf8("卸下装备"),
                                 QString::fromUtf8("请先点击一个我方英雄，再点击“卸下装备”。"));
        return;//没有选中单位时提示用户
    }

    if (!m_game->unequipUnitToInventory(m_selectedBoardUnitId)) {
        QMessageBox::information(this,
                                 QString::fromUtf8("卸下装备"),
                                 QString::fromUtf8("这个英雄目前没有装备可以卸下。"));
        m_selectedBoardUnitId = -1;//卸装失败后清空单位选择
        refreshBench();//刷新 Bench 状态
        refreshEquipment();//刷新装备栏
        return;//没有装备可卸时结束
    }

    m_unequipMode = false;//卸装成功后退出卸装模式
    m_selectedBoardUnitId = -1;//清空单位选择
    if (m_view->scene()) {
        m_view->scene()->update();//刷新单位装备角标
    }
    refreshBench();//刷新 Bench
    refreshEquipment();//刷新装备栏
    refreshHud();//刷新 HUD
}
