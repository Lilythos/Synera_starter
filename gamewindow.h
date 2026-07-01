#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QVector>
#include "core/equipment.h"

class Game;
class QEvent;
class QGraphicsView;
class QLabel;
class QObject;
class QPushButton;
class QTimer;
class QVBoxLayout;
class Unit;

class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget* parent = nullptr);
    ~GameWindow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onResetButtonClicked();
    void onAdvancePhaseButtonClicked();
    void onRefreshShopButtonClicked();
    void onUpgradePopulationButtonClicked();
    void onStarUpgradeButtonClicked();
    void onGuideButtonClicked();
    void onCombatTick();
    void onSaveButtonClicked();
    void onLoadButtonClicked();

private:
    void setupUI();
    void refreshHud();
    void refreshBench();
    void refreshShop();
    void refreshEquipment();
    void refreshSynergies();
    void refreshResultNotice();
    void refreshPanels();
    void connectUnitItemDrops();
    void selectBenchSlot(int index);
    void selectEquipmentSlot(int index);
    void selectUnequipMode();
    void updateCombatTimer();
    bool moveBenchAtBoardPosition(int benchIndex, const QPoint& boardPos);
    bool moveBoardUnitToBenchAtScreenPosition(const QPoint& boardPos, const QPoint& screenPos);
    bool equipInventoryAtBoardPosition(int inventoryIndex, const QPoint& boardPos, bool showNotice);
    void selectBoardUnitAt(const QPoint& boardPos);
    void showEquipmentAppliedNotice(EquipmentType equipmentType, Unit* unit);
    void showPostResolveNotice(int previousGold, int previousHp);
    void showGameOverNoticeIfNeeded();

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QGraphicsView* m_view;
    QLabel* m_hpLabel;
    QLabel* m_goldLabel;
    QLabel* m_populationLabel;
    QLabel* m_stageLabel;
    QLabel* m_phaseLabel;
    QLabel* m_prepTimeLabel;
    QLabel* m_resultLabel;
    QPushButton* m_resetButton;
    QPushButton* m_advancePhaseButton;
    QPushButton* m_saveButton;
    QPushButton* m_loadButton;
    QPushButton* m_guideButton;
    QVector<QPushButton*> m_benchButtons;
    QVector<QPushButton*> m_shopButtons;
    QVector<QPushButton*> m_equipmentButtons;
    QPushButton* m_refreshShopButton;
    QPushButton* m_upgradePopulationButton;
    QPushButton* m_starUpgradeButton;
    QPushButton* m_unequipButton;
    QLabel* m_synergyLabel;
    QTimer* m_combatTimer;
    int m_selectedBenchIndex;
    int m_dragBenchIndex;
    QPoint m_benchPressPos;
    int m_selectedEquipmentIndex;
    int m_dragEquipmentIndex;
    QPoint m_equipmentPressPos;
    bool m_unequipMode;
    int m_selectedBoardUnitId;
    bool m_gameOverNoticeShown;
    Game* m_game;
};

#endif // GAMEWINDOW_H
