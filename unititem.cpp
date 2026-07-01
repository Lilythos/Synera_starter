#include "gui/unititem.h"
#include "core/equipment.h"
#include "entity/unit.h"
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

namespace {

qreal safeRatio(int value, int maximum)
{//计算安全比例，用于血条和蓝条
    if (maximum <= 0) {
        return 0.0;//最大值非法时比例按 0 处理
    }

    const qreal ratio = qreal(value) / qreal(maximum);//当前值占最大值的比例
    return qBound<qreal>(0.0, ratio, 1.0);//限制在 0 到 1 之间，避免画出边界
}

void drawStatusBar(QPainter* painter,
                   const QRectF& rect,
                   qreal ratio,
                   const QColor& fillColor)
{//绘制一条状态条，如血条或蓝条
    painter->setPen(QPen(QColor(15, 15, 15), 1.0));//状态条边框
    painter->setBrush(QColor(35, 35, 35, 220));//状态条背景
    painter->drawRect(rect);//先画背景框

    QRectF fillRect = rect.adjusted(1, 1, -1, -1);//内部填充区域，留出边框
    fillRect.setWidth(fillRect.width() * ratio);//按比例缩放填充宽度

    painter->setPen(Qt::NoPen);//填充部分不画边框
    painter->setBrush(fillColor);//设置填充颜色
    painter->drawRect(fillRect);//绘制当前数值部分
}

QString stateText(Unit::State state)
{//把单位状态转换成界面显示文字
    switch (state) {
    case Unit::State::Moving:
        return QString::fromUtf8("移动");
    case Unit::State::Attacking:
        return QString::fromUtf8("攻击");
    case Unit::State::Casting:
        return QString::fromUtf8("技能");
    case Unit::State::Dead:
        return QString::fromUtf8("阵亡");
    case Unit::State::Idle:
        return QString();//空闲时不显示状态气泡
    }

    return QString();
}

QColor stateColor(Unit::State state)
{//根据单位状态选择特效颜色
    switch (state) {
    case Unit::State::Moving:
        return QColor(106, 221, 255);//移动：蓝色
    case Unit::State::Attacking:
        return QColor(255, 103, 79);//攻击：红色
    case Unit::State::Casting:
        return QColor(255, 211, 83);//施法：金色
    case Unit::State::Dead:
        return QColor(130, 122, 142);//死亡：灰紫色
    case Unit::State::Idle:
        return QColor(216, 184, 93);//空闲：默认金色
    }

    return QColor(216, 184, 93);
}

QString equipmentMarkerText(EquipmentType type)
{//把装备类型转换成单位角标文字
    switch (type) {
    case EquipmentType::IronSword:
        return QString::fromUtf8("剑");//铁剑
    case EquipmentType::Chainmail:
        return QString::fromUtf8("甲");//锁子甲
    case EquipmentType::SwiftGloves:
        return QString::fromUtf8("迅");//迅捷手套
    case EquipmentType::BlueCrystal:
        return QString::fromUtf8("蓝");//蓝晶石
    case EquipmentType::None:
        return QString();//无装备不显示角标文字
    }

    return QString();
}

} // namespace

UnitItem::UnitItem(Unit* unit, QGraphicsItem* parent)
    // 创建单位图元，保存逻辑单位指针并初始化拖拽状态。
    : QGraphicsObject(parent)
    , m_unit(unit)//对应的逻辑单位
    , m_gridPos(-1, -1)//当前逻辑格坐标，默认无效
    , m_dragging(false)//当前是否正在拖拽
{
    setAcceptedMouseButtons(Qt::LeftButton);//只响应左键拖拽
}

QRectF UnitItem::boundingRect() const
{//返回单位图元的绘制边界
    return QRectF(-48, -50, 96, 100);//包含徽章、血条、状态气泡和装备角标
}

void UnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{//绘制单位，包括徽章、名字、血条、蓝条、星级和装备标记
    painter->setRenderHint(QPainter::Antialiasing);//开启抗锯齿，让图形更圆滑

    if (!m_unit) {
        return;//没有逻辑单位时不绘制
    }

    painter->setPen(Qt::NoPen);//阴影不需要边框
    painter->setBrush(QColor(15, 10, 22, 150));//单位脚下阴影颜色
    painter->drawEllipse(QRectF(-25, 16, 50, 14));//绘制脚下阴影

    QPolygonF badge;//单位主体徽章轮廓
    badge << QPointF(0, -24)
          << QPointF(24, -10)
          << QPointF(24, 13)
          << QPointF(0, 27)
          << QPointF(-24, 13)
          << QPointF(-24, -10);

    const bool isEnemy = m_unit->owner() == Unit::Owner::EnemyCtrl;//是否为敌方单位
    const QColor effectColor = stateColor(m_unit->state());//当前状态对应的效果颜色

    if (m_unit->state() != Unit::State::Idle) {
        painter->setPen(QPen(effectColor, 2.0, Qt::DashLine));//非空闲状态显示外圈特效
        painter->setBrush(Qt::NoBrush);//只画外圈不填充
        painter->drawEllipse(QRectF(-31, -31, 62, 62));//绘制状态外圈
    }

    painter->setPen(QPen(effectColor, 2.0));//徽章边框随状态变色
    painter->setBrush(isEnemy ? QColor(94, 38, 57) : QColor(68, 42, 101));//敌我使用不同底色
    painter->drawPolygon(badge);//绘制单位主体徽章

    painter->setPen(QColor(247, 237, 197));//单位名称颜色
    QFont nameFont = painter->font();//当前字体
    nameFont.setPointSize(9);//名称字号
    nameFont.setBold(true);//名称加粗
    painter->setFont(nameFont);//应用名称字体
    QFontMetrics nameMetrics(nameFont);//用于计算文字宽度
    const QString displayName = nameMetrics.elidedText(m_unit->name(), Qt::ElideRight, 62);//过长名称自动省略
    painter->drawText(QRectF(-31, -17, 62, 28),
                      Qt::AlignCenter,
                      displayName);

    const QString state = stateText(m_unit->state());//当前状态显示文字
    if (!state.isEmpty()) {
        const QRectF stateBubble(14, -36, 36, 20);//状态气泡区域
        painter->setPen(QPen(QColor(77, 43, 12), 1.0));//气泡边框
        painter->setBrush(effectColor);//气泡底色使用状态色
        painter->drawRoundedRect(stateBubble, 7, 7);//绘制圆角状态气泡

        painter->setPen(QColor(35, 18, 12));//气泡文字颜色
        QFont bubbleFont = painter->font();//状态字体
        bubbleFont.setPointSize(8);//状态字号
        bubbleFont.setBold(true);//状态文字加粗
        painter->setFont(bubbleFont);//应用状态字体
        painter->drawText(stateBubble, Qt::AlignCenter, state);//绘制状态文字
    }

    drawStatusBar(painter,
                  QRectF(-34, -39, 68, 6),
                  safeRatio(m_unit->hp(), m_unit->maxHp()),//当前生命比例
                  QColor(91, 198, 107));//绿色血条

    drawStatusBar(painter,
                  QRectF(-34, -31, 68, 5),
                  safeRatio(m_unit->mana(), m_unit->maxMana()),//当前法力比例
                  QColor(152, 101, 219));//紫色蓝条

    painter->setPen(QColor(216, 184, 93));//星级文字颜色
    QFont starFont = painter->font();//星级字体
    starFont.setPointSize(8);//星级字号
    starFont.setBold(true);//星级加粗
    painter->setFont(starFont);//应用星级字体
    if (m_unit->owner() == Unit::Owner::PlayerCtrl) {
        painter->drawText(QRectF(-38, 25, 76, 14),
                          Qt::AlignCenter,
                          QString::number(m_unit->star()) + QString::fromUtf8("★"));//只给我方单位显示星级
    }

    if (m_unit->hasEquipment()) {
        const QRectF marker(22, 18, 16, 16);//装备角标区域
        painter->setPen(QPen(QColor(62, 43, 13), 1.0));//装备角标边框
        painter->setBrush(QColor(216, 184, 93));//装备角标底色
        painter->drawRoundedRect(marker, 2, 2);//绘制装备角标

        painter->setPen(QColor(35, 25, 15));//装备文字颜色
        QFont equipFont = painter->font();//装备角标字体
        equipFont.setPointSize(7);//装备角标字号
        equipFont.setBold(true);//装备角标加粗
        painter->setFont(equipFont);//应用装备字体
        painter->drawText(marker, Qt::AlignCenter, equipmentMarkerText(m_unit->equipment()));//显示装备首字
    }
}

int UnitItem::unitId() const
{//返回对应逻辑单位的 id
    return m_unit ? m_unit->id() : -1;//没有单位时返回无效 id
}

void UnitItem::setGridPos(const QPoint& gridPos)
{//同步单位图元当前所在格子
    m_gridPos = gridPos;//保存逻辑格坐标
    update();//坐标信息变化后刷新显示
}

void UnitItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{//鼠标按下时开始拖拽
    if (event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);//非左键交给父类处理
        return;//不启动拖拽
    }

    m_dragging = true;//标记正在拖拽
    emit dragStarted(unitId(), m_gridPos, event->scenePos());//通知 Game 拖拽开始
    event->accept();//事件已处理
}

void UnitItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{//鼠标移动时发送拖拽移动信号
    if (!m_dragging) {
        QGraphicsObject::mouseMoveEvent(event);//未拖拽时交给父类处理
        return;//不发送拖拽信号
    }

    emit dragMoved(unitId(), m_gridPos, event->scenePos());//通知 Game 更新悬停格
    event->accept();//事件已处理
}

void UnitItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{//鼠标释放时结束拖拽
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QGraphicsObject::mouseReleaseEvent(event);//未拖拽或非左键释放时交给父类
        return;//不执行落点逻辑
    }

    m_dragging = false;//结束拖拽状态
    emit dragDroppedWithScreenPos(unitId(), m_gridPos, event->scenePos(), QCursor::pos());//给 Bench/装备等屏幕区域判断使用
    emit dragDropped(unitId(), m_gridPos, event->scenePos());//给棋盘落点判断使用
    event->accept();//事件已处理
}
