#include <stdint.h>
#include <stdio.h>

// #define RETURN_ERROR (-1)
// #define RETURN_OK (0)

enum {
    LIGHT_STATE_OFF = 0,
    LIGHT_STATE_ON,
    LIGHT_STATE_BUTT
};

enum {
    EVENT_BTN_OFF = 0,
    EVENT_BTN_ON,
    EVENT_BTN_BUTT
};

static uint32_t g_stateTable[LIGHT_STATE_BUTT][EVENT_BTN_BUTT] = {
                     /* trun off */    /* trun on */
/* light off */    { LIGHT_STATE_OFF, LIGHT_STATE_ON },
/* light on  */    { LIGHT_STATE_OFF, LIGHT_STATE_ON }
};

uint32_t LightFsmEvent(uint32_t currState, uint32_t event)
{
    // if (currState >= LIGHT_STATE_BUTT || event >= EVENT_BTN_BUTT) {
    //     return RETURN_ERROR;
    // }

    return g_stateTable[currState][event];
}

int main()
{
    uint32_t index, newState;
    uint32_t state = LIGHT_STATE_OFF;
    uint32_t eventList[] = { EVENT_BTN_ON, EVENT_BTN_OFF, EVENT_BTN_ON };
    uint32_t size = sizeof(eventList) / sizeof(eventList[0]);

    for (index = 0; index < size; index++) {
        newState = LightFsmEvent(state, eventList[index]);
        printf("oldState:%u, event:%u, newState:%u\n", state, eventList[index], newState);
        state = newState;
    }

    return 0;
}

