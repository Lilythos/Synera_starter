#include "heroes.h"
#include "core/board.h"
#include "core/combat_system.h"
#include "core/targeting.h"

namespace {

void healIfFriendlyUnitAt(Board& board, Unit* source, const QPoint& pos, int amount)
{//若指定位置存在友方单位，则对其进行治疗
    if (!source || !board.isValidPosition(pos)) {
        //治疗来源为空，或目标坐标不是棋盘合法位置
        return;
    }

    Unit* target = board.getUnitAt(pos);//该位置上的单位
    if (!target || target == source) {
        //该位置没有单位或就是治疗来源
        return;
    }

    if (target->owner() != source->owner() || !target->isAlive()) {
        //不属于同一阵营或该单位已死亡
        return;
    }

    target->heal(amount);//对该友方单位进行治疗
}

void healLeftAndRight(Board& board, Unit* source, int amount)
{//治疗来源左右两侧相邻格子上的友方单位
    const QPoint pos = source->position();//治疗来源当前位置
    healIfFriendlyUnitAt(board, source, QPoint(pos.x() - 1, pos.y()), amount);//治疗左侧相邻格子
    healIfFriendlyUnitAt(board, source, QPoint(pos.x() + 1, pos.y()), amount);//治疗右侧相邻格子
}

void healAllFriendlyUnits(Board& board, Unit* source, int amount)
{//对棋盘上所有友方单位进行治疗
    for (Unit* unit : board.units()) {
        if (!unit) {
            //跳过空指针
            continue;
        }
        if (unit->owner() != source->owner() || !unit->isAlive()) {
            //不属于同一阵营或该单位已死亡
            continue;
        }
        unit->heal(amount);//对该友方单位进行治疗
    }
}

bool damageEnemyUnitAt(Board& board, Unit* source, const QPoint& pos, int amount)
{//若指定位置存在敌方单位，则对其造成伤害
    if (!source || !board.isValidPosition(pos) || amount <= 0) {
        //伤害来源为空、坐标不合法或伤害数值无效
        return false;
    }

    Unit* target = board.getUnitAt(pos);//该位置上的单位
    if (!target || target->owner() == source->owner() || !target->isAlive()) {
        //该位置没有单位、属于同一阵营或该单位已死亡
        return false;
    }

    target->takeDamage(amount);//对该敌方单位造成伤害
    return true;
}

} // namespace

Gunner::Gunner()
    : Unit(QString::fromUtf8("枪手"),
           220,
           20,
           3,
           60,
           Unit::Owner::PlayerCtrl,
           QStringList({QString::fromUtf8("枪手"), QString::fromUtf8("战意")}),
           1)
    , m_attackCount(0)//攻击次数计数器，初始为0
{
    setAttackIntervalMs(1000);
    setMoveIntervalMs(300);
}

int Gunner::basicAttackDamage()
{//计算枪手普通攻击的伤害，每第4次攻击触发双倍伤害
    ++m_attackCount;

    if (m_attackCount % 4 == 0) {
        return atk() * 2;
    }

    return atk();
}

bool Gunner::castSkill(Board& board, const QVector<Unit*>& units)
{//枪手释放技能：对选中目标造成三倍攻击力的伤害
    (void)units;

    Unit* target = Targeting::selectTarget(this, board);//选取技能目标
    if (!target) {
        return false;
    }

    const int damage = atk() * 3;//技能伤害为三倍攻击力
    const QPoint targetPosition = target->position();//记录目标位置，便于后续记录行动
    target->takeDamage(damage);//对目标造成伤害
    setLastAction(ActionType::Attack);//记录最近一次行动为攻击
    board.recordAction(position(), ActionType::Attack, damage, targetPosition);//在棋盘上记录本次攻击行动
    return true;
}

Healer::Healer()
    : Unit(QString::fromUtf8("治疗师"),
           240,
           10,
           2,
           60,
           Unit::Owner::PlayerCtrl,
           QStringList({QString::fromUtf8("治疗师")}),
           1)
    , m_moveStepCount(0)//移动步数计数器，初始为0
{
    setAttackIntervalMs(1200);
    setMoveIntervalMs(300);
}

void Healer::onAfterMoveStep(Board& board, const QVector<Unit*>& units)
{//治疗师每移动一步后触发的回调，每移动两步治疗一次左右友方单位
    (void)units;

    ++m_moveStepCount;
    if (m_moveStepCount % 2 != 0) {
        return;
    }

    healLeftAndRight(board, this, 3);//治疗左右相邻友方单位
    setLastAction(ActionType::Heal);//记录最近一次行动为治疗
    board.recordAction(position(), ActionType::Heal, 3, QPoint(-1, -1));//在棋盘上记录本次治疗行动
}

bool Healer::castSkill(Board& board, const QVector<Unit*>& units)
{//治疗师释放技能：治疗全体友方单位
    (void)units;

    const int healAmount = 25;
    healAllFriendlyUnits(board, this, healAmount);//治疗所有友方单位
    setLastAction(ActionType::Heal);//记录最近一次行动为治疗
    board.recordAction(position(), ActionType::Heal, healAmount, QPoint(-1, -1));//在棋盘上记录本次治疗行动
    return true;
}

WarGod::WarGod()
    : Unit(QString::fromUtf8("战神"),
           500,
           50,
           1,
           80,
           Unit::Owner::PlayerCtrl,
           QStringList({QString::fromUtf8("战意")}),
           1)
    , m_moveStepCount(0)
{
    setAttackIntervalMs(1100);
    setMoveIntervalMs(200);
}

void WarGod::onAfterMoveStep(Board& board, const QVector<Unit*>& units)
{//战神每移动一步后触发的回调，每移动两步治疗一次左右友方单位
    (void)units;

    ++m_moveStepCount;
    if (m_moveStepCount % 2 != 0) {
        return;
    }

    healLeftAndRight(board, this, 5);
    setLastAction(ActionType::Heal);
    board.recordAction(position(), ActionType::Heal, 5, QPoint(-1, -1));
}

bool WarGod::castSkill(Board& board, const QVector<Unit*>& units)
{//战神释放技能：对目标及其周围十字范围内的敌方单位造成范围伤害
    (void)units;

    Unit* target = Targeting::selectTarget(this, board);//选取技能目标
    if (!target) {
        return false;
    }

    const int damage = atk() * 2;//技能伤害为两倍攻击力
    const QPoint center = target->position();//以目标位置为范围伤害中心
    bool hitAny = false;//标记是否实际命中过任意单位

    hitAny = damageEnemyUnitAt(board, this, center, damage) || hitAny;//对中心位置造成伤害
    hitAny = damageEnemyUnitAt(board, this, QPoint(center.x() - 1, center.y()), damage) || hitAny;//对左侧相邻位置造成伤害
    hitAny = damageEnemyUnitAt(board, this, QPoint(center.x() + 1, center.y()), damage) || hitAny;//右
    hitAny = damageEnemyUnitAt(board, this, QPoint(center.x(), center.y() - 1), damage) || hitAny;//上
    hitAny = damageEnemyUnitAt(board, this, QPoint(center.x(), center.y() + 1), damage) || hitAny;//下

    if (!hitAny) {
        //没有命中任何单位，技能视为释放失败
        return false;
    }

    setLastAction(ActionType::Attack);
    board.recordAction(position(), ActionType::Attack, damage, center);
    return true;
}

Martyr::Martyr()
    : Unit(QString::fromUtf8("殉道者"),
           260,
           20,
           1,
           60,
           Unit::Owner::PlayerCtrl,
           QStringList({QString::fromUtf8("殉道者"), QString::fromUtf8("战意")}),
           1)
    , m_moveStepCount(0)
{
    setAttackIntervalMs(1200);
    setMoveIntervalMs(300);
}

void Martyr::onAfterMoveStep(Board& board, const QVector<Unit*>& units)
{//殉道者每移动一步后触发的回调，每移动两步治疗一次左右友方单位
    (void)units;

    ++m_moveStepCount;//移动步数加一
    if (m_moveStepCount % 2 != 0) {
        return;
    }

    healLeftAndRight(board, this, 3);
    setLastAction(ActionType::Heal);
    board.recordAction(position(), ActionType::Heal, 3, QPoint(-1, -1));
}

void Martyr::onDeath(Board& board, const QVector<Unit*>& units)
{//殉道者死亡时触发的回调，死亡后治疗全体友方单位
    (void)units;

    const int healAmount = 5 + synergyDeathHealBonus();//死亡治疗量为基础值加上羁绊带来的额外加成
    healAllFriendlyUnits(board, this, healAmount);//治疗所有友方单位
    setLastAction(ActionType::DeathHeal);
    board.recordAction(position(), ActionType::DeathHeal, healAmount, QPoint(-1, -1));
}

Mimic::Mimic()
    : Unit(QString::fromUtf8("模仿者"),
           220,
           15,
           2,
           60,
           Unit::Owner::PlayerCtrl,
           QStringList({QString::fromUtf8("模仿者")}),
           1)
{
    setAttackIntervalMs(1200);
    setMoveIntervalMs(300);
}

bool Mimic::onBeforeCombatAction(Board& board, const QVector<Unit*>& /*units*/)
{//模仿者在执行战斗行动前触发的回调，复制右侧相邻单位上一次的行动
    const QPoint pos = position();//模仿者当前位置
    const QPoint rightPos(pos.x() + 1, pos.y());//右侧相邻格子的位置
    if (!board.isValidPosition(rightPos)) {
        //右侧相邻位置超出棋盘范围
        return false;
    }

    const Board::ActionRecord action = board.lastActionRecordAt(rightPos);//该位置上记录的上一次行动
    if (action.type == ActionType::None) {
        //该位置没有记录任何行动，无法模仿
        return false;
    }

    if (action.type == ActionType::Attack) {
        //模仿攻击行动
        Unit* target = Targeting::selectTarget(this, board);//选取攻击目标
        if (!target) {
            //没有可选目标，模仿失败
            return false;
        }

        return CombatSystem::basicAttack(this, target, &board);//对目标执行一次普通攻击
    }

    if (action.type == ActionType::Heal) {
        //模仿治疗行动
        const int healAmount = action.amount > 0 ? action.amount : 3;//使用原行动的治疗量，若无效则使用默认值3
        healLeftAndRight(board, this, healAmount);
        setLastAction(ActionType::Heal);
        board.recordAction(position(), ActionType::Heal, healAmount, QPoint(-1, -1));
        return true;
    }

    if (action.type == ActionType::DeathHeal) {
        //模仿死亡治疗行动
        const int healAmount = action.amount > 0 ? action.amount : 5;//使用原行动的治疗量，若无效则使用默认值5
        healAllFriendlyUnits(board, this, healAmount);
        setLastAction(ActionType::DeathHeal);
        board.recordAction(position(), ActionType::DeathHeal, healAmount, QPoint(-1, -1));
        return true;
    }

    return false;//未知的行动类型，模仿失败
}