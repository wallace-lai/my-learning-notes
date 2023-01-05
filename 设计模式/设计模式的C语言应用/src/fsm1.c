#include <stdio.h>
#include <stdint.h>

enum {
    LIGHT_STATE_OFF,    // 灯灭
    LIGHT_STATE_LOW,    // 低亮度
    LIGHT_STATE_HIGH,   // 高亮度
    LIGHT_STATE_PEND,   // 未定义
    LIGHT_STATE_BUTT
};

enum {
    EVENT_BTN_OFF,      // 关灯
    EVENT_BTN_ON,       // 开灯
    EVENT_BTN_BUTT
};

// 记录上一次灯的亮度
static uint32_t g_lastState = LIGHT_STATE_LOW;

typedef struct tagLightFsm {
    uint32_t (*oper)(void *context);    // 事件处理函数
    uint32_t nextState;                 // 下一个状态
} LightFsm;

uint32_t FsmIgnore(void *context)
{
    printf("no state changed, ignore.\n");
    return 0;
}

uint32_t FsmButtonOn(void *context)
{
    uint32_t nextState;
    uint32_t lastState = *((uint32_t *)context);

    // 此次开灯后的亮度取决于上一次开灯后的亮度
    if (lastState == LIGHT_STATE_LOW) {
        nextState = LIGHT_STATE_HIGH;
    } else if (lastState == LIGHT_STATE_HIGH) {
        nextState = LIGHT_STATE_LOW;
    }

    // do something

    return nextState;
}

uint32_t FsmButtonOff(void *context)
{
    printf("light turned off.\n");
    return 0;
}

static LightFsm g_lightFsm[LIGHT_STATE_BUTT][EVENT_BTN_BUTT] = {
    // LIGHT_STATE_OFF
    { FsmIgnore, LIGHT_STATE_OFF },     // EVENT_BTN_OFF
    { FsmButtonOn, LIGHT_STATE_PEND },  // EVENT_BTN_ON
    // LIGHT_STATE_LOW
    { FsmButtonOff, LIGHT_STATE_OFF },  // EVENT_BTN_OFF
    { FsmIgnore, LIGHT_STATE_LOW },     // EVENT_BTN_ON
    // LIGHT_STATE_HIGH
    { FsmButtonOff, LIGHT_STATE_OFF },  // EVENT_BTN_OFF
    { FsmIgnore, LIGHT_STATE_HIGH }     // EVENT_BTN_ON
};

int32_t LightFsmEvent(uint32_t currState, uint32_t event)
{
    uint32_t nextState;
    uint32_t nextStateTmp;

    if (g_lightFsm[currState][event].nextState != LIGHT_STATE_PEND) {
        // 状态转换表中的下一步状态不为pend，说明下一步状态是确定的
        (void)g_lightFsm[currState][event].oper(&g_lastState);
        nextState = g_lightFsm[currState][event].nextState;
    } else {
        // 状态转换表中的下一步状态为pend，则实际下一步状态需要计算得到
        nextState = g_lightFsm[currState][event].oper(&g_lastState);
    }

    // 更新g_lastState
    if (nextState == LIGHT_STATE_HIGH || nextState == LIGHT_STATE_LOW) {
        g_lastState = nextState;
    }
}

