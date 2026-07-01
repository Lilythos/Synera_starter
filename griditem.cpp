#include "griditem.h"
#include <QGraphicsSceneHoverEvent>
#include <QPainter>

GridItem::GridItem(int row, int col, const QPolygonF& polygon, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_row(row)//逻辑行号
    , m_col(col)//逻辑列号
    , m_polygon(polygon)//实际绘制用的六边形/格子轮廓
    , m_bounds(polygon.boundingRect())//图元边界，用于 Qt 判断重绘区域
    , m_baseColor(QColor(60, 60, 80))//默认底色
    , m_hoverActive(false)//拖拽经过高亮
    , m_dropActive(false)//可放置高亮
    , m_pointerHover(false)//鼠标悬停高亮
{
    setAcceptHoverEvents(true);//允许接收鼠标进入/离开事件
}

QRectF GridItem::boundingRect() const
{//返回图元的绘制边界
    return m_bounds.adjusted(-2.0, -2.0, 2.0, 2.0);//额外扩一点，避免边框被裁剪
}

void GridItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{//绘制棋盘格本体和交互高亮
    QColor fill = m_baseColor;//当前填充色
    QColor border = QColor(40, 40, 40);//默认边框色

    if (m_dropActive) {
        fill = QColor(110, 170, 110);//可放置时使用绿色提示
        border = QColor(100, 255, 100);//可放置边框更亮
    } else if (m_hoverActive || m_pointerHover) {
        fill = m_baseColor.lighter(120);//普通悬停时提高亮度
    }

    painter->setPen(QPen(border, 2));//设置边框画笔
    painter->setBrush(fill);//设置填充颜色
    painter->drawPolygon(m_polygon);//绘制格子多边形

    if (m_hoverActive || m_pointerHover) {
        painter->setPen(QPen(QColor(220, 220, 220), 2));//悬停时再画一层浅色描边
        painter->setBrush(Qt::NoBrush);//只画边框，不覆盖填充
        painter->drawPolygon(m_polygon);//绘制高亮轮廓
    }
}

QPoint GridItem::gridPos() const
{//返回该图元对应的逻辑棋盘坐标
    return QPoint(m_col, m_row);//x 表示列，y 表示行
}

void GridItem::setBaseColor(const QColor& color)
{//设置棋盘格基础颜色
    m_baseColor = color;//保存新颜色
    update();//通知 Qt 重绘
}

void GridItem::setHoverActive(bool active)
{//设置拖拽悬停状态
    if (m_hoverActive == active) {
        return;//状态没变，不需要重绘
    }
    m_hoverActive = active;//更新悬停状态
    update();//刷新显示
}

void GridItem::setDropActive(bool active)
{//设置是否显示可放置状态
    if (m_dropActive == active) {
        return;//状态没变，不需要重绘
    }
    m_dropActive = active;//更新可放置状态
    update();//刷新显示
}

void GridItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{//鼠标进入格子时触发
    Q_UNUSED(event);//当前不需要使用事件对象
    if (!m_pointerHover) {
        m_pointerHover = true;//记录鼠标悬停
        update();//刷新高亮
    }
}

void GridItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{//鼠标离开格子时触发
    Q_UNUSED(event);//当前不需要使用事件对象
    if (m_pointerHover) {
        m_pointerHover = false;//取消鼠标悬停
        update();//刷新高亮
    }
}
