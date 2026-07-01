#include "pathfinder.h"

#include <algorithm>

namespace {

int toIndex(const QPoint& pos)
{//将二维坐标转换为一维数组下标
    return pos.y() * Board::COLS + pos.x();
}

QPoint fromIndex(int index)
{//将一维数组下标转换为二维坐标
    return QPoint(index % Board::COLS, index / Board::COLS);
}

QVector<QPoint> neighborPositions(const QPoint& pos)
{//获取上下左右四个方向的相邻坐标
    QVector<QPoint> neighbors;//存放四个邻居坐标的容器
    neighbors.reserve(4);//预留4个元素的空间

    neighbors.append(QPoint(pos.x(), pos.y() - 1));//上
    neighbors.append(QPoint(pos.x(), pos.y() + 1));//下
    neighbors.append(QPoint(pos.x() - 1, pos.y()));//左
    neighbors.append(QPoint(pos.x() + 1, pos.y()));//右

    return neighbors;
}

QVector<QPoint> reconstructPath(const QVector<int>& parent, int startIndex, int goalIndex)
{//根据父节点数组从终点回溯到起点，还原完整路径
    QVector<QPoint> path;//存放回溯得到的路径坐标

    int current = goalIndex;//从终点下标开始回溯
    while (current != -1) {
        path.append(fromIndex(current));//将当前下标转换为坐标并加入路径
        if (current == startIndex) {
            break;
        }
        current = parent[current];//跳转到父节点继续回溯
    }

    //路径为空或未能回溯到起点，说明路径无效
    if (path.isEmpty() || toIndex(path.last()) != startIndex) {
        return QVector<QPoint>();
    }

    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace

QVector<QPoint> Pathfinder::findPath(const Board& board,
                                     const QPoint& start,
                                     const QPoint& goal)
{//使用广度优先搜索寻找从起点到终点的最短路径
    if (!board.isValidPosition(start) || !board.isValidPosition(goal)) {
        return QVector<QPoint>();
    }

    if (start == goal) {
        return QVector<QPoint>({start});
    }

    if (board.hasUnitAt(goal)) {
        return QVector<QPoint>();
    }

    const int cellCount = Board::ROWS * Board::COLS;//棋盘总格子数
    const int startIndex = toIndex(start);//起点对应的一维下标
    const int goalIndex = toIndex(goal);//终点对应的一维下标

    QVector<bool> visited(cellCount, false);//记录每个格子是否已被访问
    QVector<int> parent(cellCount, -1);//记录每个格子的来源格子下标，用于回溯路径
    QVector<int> queue;//广度优先搜索使用的队列
    queue.reserve(cellCount);//预留最大可能的容量

    visited[startIndex] = true;//标记起点为已访问
    queue.append(startIndex);//将起点放入队列

    int head = 0;//队列头部指针，指向下一个待处理的元素
    while (head < queue.size()) {
        const int currentIndex = queue[head++];//取出当前处理的格子下标
        const QPoint currentPos = fromIndex(currentIndex);//当前格子对应的坐标

        const QVector<QPoint> neighbors = neighborPositions(currentPos);//当前格子的四个相邻坐标
        for (const QPoint& nextPos : neighbors) {
            if (!board.isValidPosition(nextPos)) {
                continue;
            }

            const int nextIndex = toIndex(nextPos);//相邻坐标对应的一维下标
            if (visited[nextIndex]) {
                continue;
            }

            if (board.hasUnitAt(nextPos)) {
                continue;
            }

            visited[nextIndex] = true;//标记该格子为已访问
            parent[nextIndex] = currentIndex;//记录来源格子，便于回溯路径

            if (nextIndex == goalIndex) {
                return reconstructPath(parent, startIndex, goalIndex);//已经搜索到终点，直接回溯并返回路径
            }

            queue.append(nextIndex);//将该格子加入队列等待后续扩展
        }
    }

    return QVector<QPoint>();
}

bool Pathfinder::moveUnitOneStep(Board& board, Unit* unit, const QPoint& goal)
{//让单位朝目标位置沿最短路径移动一步
    if (!unit || !board.containsUnit(unit)){
        return false;
    }

    const QPoint start = board.positionOf(unit);//单位当前所在的位置
    const QVector<QPoint> path = findPath(board, start, goal);//计算起点到目标的最短路径

    if (path.isEmpty()) {
        return false;
    }

    if (path.size() == 1) {
        return true;
    }

    const bool moved = board.moveUnit(unit, path.at(1));//尝试将单位移动到路径上的下一格
    if (moved) {
        unit->onAfterMoveStep(board, board.units());//移动成功后触发单位的移动后回调
    }

    return moved;   
}