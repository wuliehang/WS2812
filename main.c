#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "pwm.h"
#include "ws2812.h"
#include "stmflash.h" 
#include <string.h>
#define GROUP_COUNT_MAX         15
#define ADC_THRESHOLD_COUNT_MAX 10
//#define LED_COUNT               2100
#define BREATH_STEPS            50

#define FLASH_SAVE_ADDR         ADDR_FLASH_SECTOR_11

typedef struct __attribute__((packed)) 
{
    uint8_t baseColors[GROUP_COUNT_MAX][3];
    uint16_t adcThresholds[ADC_THRESHOLD_COUNT_MAX];
    uint8_t thresholdColors[ADC_THRESHOLD_COUNT_MAX][3];
    uint8_t groupCount;             // 实际用多少组颜色
    uint8_t thresholdCount;         // 实际用多少个阈值
		uint16_t breathDelayMs;         // 呼吸延时参数（单位ms）
		uint16_t ledCount;                // 新增：LED数量
} AppConfig_t;

static AppConfig_t appConfig;


// 保存整个配置到 Flash
static void SaveConfig(void)
{
	printf("\r\n开始保存 \r\n");
	STMFLASH_Write(FLASH_SAVE_ADDR, (uint32_t*)&appConfig, (sizeof(AppConfig_t) + 3) / 4);
}


static void ProcessSerialCommands(void)
{
    if (!(USART_RX_STA & 0x8000)) return;

    uint16_t len = USART_RX_STA & 0x3FFF;
    if (len >= USART_REC_LEN) len = USART_REC_LEN - 1;
    USART_RX_BUF[len] = '\0';

    // 1. 去掉字符串末尾的\r\n，方便命令精确匹配
    char *p = (char *)USART_RX_BUF;
    while (*p && *p != '\r' && *p != '\n') p++;
    *p = '\0';

    int idx, r, g, b, val;

    // ================== 无参数命令 =======================
    if (strcmp((char*)USART_RX_BUF, "HELP") == 0)
    {
        // 帮助命令
        printf("\r\n命令列表 (用法: 发送命令字符串到串口)：\r\n");
        printf("HELP                  - 显示命令列表\r\n");
        printf("SHOWCFG               - 显示当前所有配置\r\n");
        printf("SETGROUPCOUNT <n>     - 设置组数 (1~%d)\r\n", GROUP_COUNT_MAX);
        printf("SETTHRESHCOUNT <n>    - 设置阈值数量 (1~%d)\r\n", ADC_THRESHOLD_COUNT_MAX);
        printf("SETCOLOR <idx> <R> <G> <B>      - 设置指定组的基色 (组号, R, G, B)\r\n");
        printf("SETTHRESH <idx> <val>           - 设置指定编号的ADC阈值\r\n");
        printf("SETTCOLOR <idx> <R> <G> <B>     - 设置指定阈值的颜色\r\n");
        printf("ADDGROUP <R> <G> <B>            - 添加新组并指定基色\r\n");
        printf("DELGROUP <idx>                  - 删除指定组\r\n");
        printf("ADDTHRESH <val> <R> <G> <B>     - 添加新阈值及其颜色\r\n");
        printf("DELTHRESH <idx>                 - 删除指定阈值\r\n");
				printf("SETDELAY <ms>         - 设置呼吸灯延时时间（单位ms，0~1000）\r\n");
				printf("SETLEDCOUNT <n>       - 设置LED灯珠数量 (1~2100)\r\n");
        printf("\r\n说明：组号/阈值编号从0开始计数。\r\n");
    }
    else if (strcmp((char*)USART_RX_BUF, "SHOWCFG") == 0)
    {
        // 显示配置命令
        printf("\r\n------ 当前配置 ------\r\n");
        printf("实际组数 groupCount=%d，实际阈值数 thresholdCount=%d\r\n", appConfig.groupCount, appConfig.thresholdCount);
        printf("各组基色:\r\n");
        for (int i = 0; i < appConfig.groupCount; i++) 
        {
            printf("  G%d: R=%d G=%d B=%d\r\n", i,
                appConfig.baseColors[i][0],
                appConfig.baseColors[i][1],
                appConfig.baseColors[i][2]);
        }
        printf("\r\n各ADC阈值及颜色:\r\n");
        for (int i = 0; i < appConfig.thresholdCount; i++) 
        {
            printf("  T%d: Threshold=%d, Color=R%d G%d B%d\r\n", i,
                appConfig.adcThresholds[i],
                appConfig.thresholdColors[i][0],
                appConfig.thresholdColors[i][1],
                appConfig.thresholdColors[i][2]);
        }
				printf("\r\n当前呼吸灯延时参数: %d ms\r\n", appConfig.breathDelayMs);
				printf("\r\n当前LED数量: %d\r\n", appConfig.ledCount);
        printf("---------------------\r\n");
    }

    // ================== 带参数命令 =======================
    else if (sscanf((char*)USART_RX_BUF, "SETGROUPCOUNT %d", &val) == 1)
    {
        if (val >= 1 && val <= GROUP_COUNT_MAX) {
            appConfig.groupCount = val;
            SaveConfig();
            printf("\r\n已设置组数为 %d\r\n", val);
        } else {
            printf("\r\n组数范围: 1~%d\r\n", GROUP_COUNT_MAX);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "SETTHRESHCOUNT %d", &val) == 1)
    {
        if (val >= 1 && val <= ADC_THRESHOLD_COUNT_MAX) {
            appConfig.thresholdCount = val;
            SaveConfig();
            printf("\r\n已设置阈值数为 %d\r\n", val);
        } else {
            printf("\r\n阈值数范围: 1~%d\r\n", ADC_THRESHOLD_COUNT_MAX);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "SETCOLOR %d %d %d %d", &idx, &r, &g, &b) == 4)
    {
        if (idx >= 0 && idx < appConfig.groupCount && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) 
					{
            appConfig.baseColors[idx][0] = r;
            appConfig.baseColors[idx][1] = g;
            appConfig.baseColors[idx][2] = b;
            SaveConfig();
            printf("\r\n组 %d 颜色设置为: R=%d G=%d B=%d\r\n", idx, r, g, b);
        } else {
            printf("\r\n参数错误：组 0~%d，RGB 0~255\r\n", appConfig.groupCount - 1);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "SETTHRESH %d %d", &idx, &val) == 2)
    {
        if (idx >= 0 && idx < appConfig.thresholdCount) {
            appConfig.adcThresholds[idx] = (uint16_t)val;
            SaveConfig();
            printf("\r\n阈值 %d 设置为 %d\r\n", idx, val);
        } else {
            printf("\r\n编号范围 0~%d\r\n", appConfig.thresholdCount - 1);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "SETTCOLOR %d %d %d %d", &idx, &r, &g, &b) == 4)
    {
        if (idx >= 0 && idx < appConfig.thresholdCount && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
            appConfig.thresholdColors[idx][0] = r;
            appConfig.thresholdColors[idx][1] = g;
            appConfig.thresholdColors[idx][2] = b;
            SaveConfig();
            printf("\r\n阈值 %d 颜色设置为: R=%d G=%d B=%d\r\n", idx, r, g, b);
        } else {
            printf("\r\n用法: SETTCOLOR 0~%d R G B\r\n", appConfig.thresholdCount - 1);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "DELGROUP %d", &idx) == 1)
    {
        if (idx >= 0 && idx < appConfig.groupCount)
        {
            for (int i = idx; i < appConfig.groupCount - 1; i++)
                for (int k = 0; k < 3; k++)
                    appConfig.baseColors[i][k] = appConfig.baseColors[i+1][k];
            appConfig.groupCount--;
            SaveConfig();
            printf("\r\n已删除组 %d, 当前组数=%d\r\n", idx, appConfig.groupCount);
        }
        else
            printf("\r\n组编号范围 0~%d\r\n", appConfig.groupCount-1);
    }
    else if (sscanf((char*)USART_RX_BUF, "DELTHRESH %d", &idx) == 1)
    {
        if (idx >= 0 && idx < appConfig.thresholdCount)
        {
            for (int i = idx; i < appConfig.thresholdCount - 1; i++)
            {
                appConfig.adcThresholds[i] = appConfig.adcThresholds[i+1];
                for (int k = 0; k < 3; k++)
                    appConfig.thresholdColors[i][k] = appConfig.thresholdColors[i+1][k];
            }
            appConfig.thresholdCount--;
            SaveConfig();
            printf("\r\n已删除阈值 %d, 当前阈值数=%d\r\n", idx, appConfig.thresholdCount);
        }
        else
            printf("\r\n阈值编号范围 0~%d\r\n", appConfig.thresholdCount-1);
    }
    else if (sscanf((char*)USART_RX_BUF, "ADDGROUP %d %d %d", &r, &g, &b) == 3)
    {
        if (appConfig.groupCount < GROUP_COUNT_MAX)
        {
            appConfig.baseColors[appConfig.groupCount][0] = r;
            appConfig.baseColors[appConfig.groupCount][1] = g;
            appConfig.baseColors[appConfig.groupCount][2] = b;
            appConfig.groupCount++;
            SaveConfig();
            printf("\r\n已添加组 %d: R=%d G=%d B=%d\r\n", appConfig.groupCount-1, r, g, b);
        }
        else
            printf("\r\n组数已达上限=%d\r\n", GROUP_COUNT_MAX);
    }
    else if (sscanf((char*)USART_RX_BUF, "ADDTHRESH %d %d %d %d", &val, &r, &g, &b) == 4)
    {
        if (appConfig.thresholdCount < ADC_THRESHOLD_COUNT_MAX)
        {
            appConfig.adcThresholds[appConfig.thresholdCount] = val;
            appConfig.thresholdColors[appConfig.thresholdCount][0] = r;
            appConfig.thresholdColors[appConfig.thresholdCount][1] = g;
            appConfig.thresholdColors[appConfig.thresholdCount][2] = b;
            appConfig.thresholdCount++;
            SaveConfig();
            printf("\r\n已添加阈值 %d: 阈值=%d 颜色=R%d G%d B%d\r\n",
                appConfig.thresholdCount-1, val, r, g, b);
        }
        else
            printf("\r\n阈值数已达上限=%d\r\n", ADC_THRESHOLD_COUNT_MAX);
    }
		
		else if (sscanf((char*)USART_RX_BUF, "SETDELAY %d", &val) == 1)
		{
				if (val >= 0 && val <= 1000) { // 合理范围防呆
						appConfig.breathDelayMs = val;
						SaveConfig();
						printf("\r\n已设置呼吸延时为 %d ms\r\n", val);
				} else {
						printf("\r\n呼吸延时设置范围: 0~1000 ms\r\n");
				}
		}
		else if (sscanf((char*)USART_RX_BUF, "SETLEDCOUNT %d", &val) == 1)
		{
				if (val >= 1 && val <= 2100) { // 视你硬件能力设定最大支持
						appConfig.ledCount = val;
					ws2812_clear(appConfig.ledCount);
						SaveConfig();
						printf("\r\n已设置LED数量为 %d\r\n", val);
				} else {
						printf("\r\nLED数量设置范围: 1~2100\r\n");
				}
		}
    // ================== 其他未知命令 ======================
    else
    {
        printf("\r\n未知命令，请发送 HELP 查看支持的命令列表。\r\n");
    }

    // 清空输入缓冲区
    memset(USART_RX_BUF, 0, 10);
    USART_RX_STA = 0;
}



static void ShowConfig(void)
{
        printf("\r\n------ Current Config ------\r\n");
        printf("实际组数 groupCount=%d，实际阈值数 thresholdCount=%d\r\n", appConfig.groupCount, appConfig.thresholdCount);
        printf("Breath Base Colors (Group):\r\n");
        for (int i = 0; i < appConfig.groupCount; i++) {
            printf("  G%d: R=%d G=%d B=%d\r\n", i,
                   appConfig.baseColors[i][0],
                   appConfig.baseColors[i][1],
                   appConfig.baseColors[i][2]);
        }
        printf("\r\nADC Thresholds & Colors:\r\n");
        for (int i = 0; i < appConfig.thresholdCount; i++) {
            printf("  T%d: Threshold=%d, Color=R%d G%d B%d\r\n", i,
                   appConfig.adcThresholds[i],
                   appConfig.thresholdColors[i][0],
                   appConfig.thresholdColors[i][1],
                   appConfig.thresholdColors[i][2]);
        }
								printf("\r\n当前呼吸灯延时参数: %d ms\r\n", appConfig.breathDelayMs);
				printf("\r\n当前LED数量: %d\r\n", appConfig.ledCount);
        printf("---------------------------\r\n");
    
}



// 读配置
static void LoadConfig(void)
{
    STMFLASH_Read(FLASH_SAVE_ADDR, (uint32_t*)&appConfig, (sizeof(AppConfig_t) + 3) / 4);

    // 检查是否全为0xFF，仅首次写入才初始化
    int allff = 1;
    uint8_t *p = (uint8_t*)&appConfig;
    for (int i = 0; i < sizeof(AppConfig_t); ++i) {
        if (p[i] != 0xFF) { allff = 0; break; }
    }
    if (allff) {
        // 只在第一次全空时初始化
        for (int i = 0; i < GROUP_COUNT_MAX; ++i) {
            appConfig.baseColors[i][0] = 150;
            appConfig.baseColors[i][1] = 150;
            appConfig.baseColors[i][2] = 150;
        }
        for (int i = 0; i < ADC_THRESHOLD_COUNT_MAX; ++i) {
            appConfig.adcThresholds[i] = 200 * (i + 1);
            uint8_t colorPreset[5][3] = { {255,0,0}, {0,255,0}, {0,0,255}, {255,255,0}, {255,0,255} };
            for (int k = 0; k < 3; k++) appConfig.thresholdColors[i][k] = colorPreset[i][k];
        }
				appConfig.ledCount = 2100;     // 默认2100
				appConfig.breathDelayMs = 0; // 默认20ms呼吸灯延时
        appConfig.groupCount = GROUP_COUNT_MAX;
        appConfig.thresholdCount = ADC_THRESHOLD_COUNT_MAX;
        SaveConfig();
    }

    // 非首次，只做有效范围容错保护，不再初始化
    if (appConfig.groupCount == 0 || appConfig.groupCount > GROUP_COUNT_MAX)
        appConfig.groupCount = GROUP_COUNT_MAX;
    if (appConfig.thresholdCount == 0 || appConfig.thresholdCount > ADC_THRESHOLD_COUNT_MAX)
        appConfig.thresholdCount = ADC_THRESHOLD_COUNT_MAX;
		if (appConfig.ledCount == 0 || appConfig.ledCount > 3000)
    appConfig.ledCount = 2100;
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);
    ws2812_init();
    uart_init(115200);

    LoadConfig();
    ws2812_clear(appConfig.ledCount);

    ShowConfig();

    static int lastThresholdIdx = -2;
    static int groupIndex = 0;
    static int step = 0;
    static int dir = 1;
    static int lastMode = -1;    // -1:未定义, 0:呼吸, 1:阈值

    while (1)
    {
        ProcessSerialCommands();

        uint16_t adcValue = 300; // TODO: 换成你的ADC采样值
        uint8_t curColor[3] = {0};
        int found = 0;
        int curThresholdIdx = -1;

        // 只判断实际阈值数量
        for (int i = appConfig.thresholdCount - 1; i >= 0; --i) {
            if (adcValue >= appConfig.adcThresholds[i]) {
                for (int k = 0; k < 3; k++)
                    curColor[k] = appConfig.thresholdColors[i][k];
                found = 1;
                curThresholdIdx = i;
                break;
            }
        }

        // 检测是否切换模式
        int curMode = found ? 1 : 0;    // 1:阈值, 0:呼吸
        if (curMode != lastMode) {
            if (curMode == 0) {  // 切回呼吸灯
                step = 0;
                dir = 1;
                // groupIndex 可保留原值，也可重置，看你的需求
            }
            lastMode = curMode;
        }

        // 生成显示色
        if (!found) {
            curColor[0] = (appConfig.baseColors[groupIndex][0] * step) / BREATH_STEPS;
            curColor[1] = (appConfig.baseColors[groupIndex][1] * step) / BREATH_STEPS;
            curColor[2] = (appConfig.baseColors[groupIndex][2] * step) / BREATH_STEPS;
        }

        for (uint16_t i = 1; i <= appConfig.ledCount; i++)
            ws2812_setPixelColor(curColor, i);
        ws2812_show(appConfig.ledCount);

        if (!found) {
            step += dir;
            if (step >= BREATH_STEPS) {
                step = BREATH_STEPS;
                dir = -1;
            } else if (step <= 0) {
                step = 0;
                dir = 1;
                groupIndex++;
                if (groupIndex >= appConfig.groupCount)
                    groupIndex = 0;
                // 每切换到下一组，呼吸重新从最暗渐亮
                step = 0;
                dir = 1;
            }
            delay_ms(appConfig.breathDelayMs);  // 呼吸灯刷新速率（自行调节）
        } else {
            step = BREATH_STEPS;
            dir = 1;
//            delay_ms(60);  // 固定灯显示时刷新速率（自行调节）
        }

        // 只在阈值变化/模式变化时打印一次
        if (curThresholdIdx != lastThresholdIdx) {
            if (curThresholdIdx >= 0)
                printf("当前处于第%d个阈值: 阈值=%d, ADC=%d\r\n", curThresholdIdx, appConfig.adcThresholds[curThresholdIdx], adcValue);
            else
                printf("当前为呼吸灯模式, ADC=%d\r\n", adcValue);
            lastThresholdIdx = curThresholdIdx;
        }
    }
}
