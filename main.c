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
    uint8_t groupCount;             // ʵ���ö�������ɫ
    uint8_t thresholdCount;         // ʵ���ö��ٸ���ֵ
		uint16_t breathDelayMs;         // ������ʱ��������λms��
		uint16_t ledCount;                // ������LED����
} AppConfig_t;

static AppConfig_t appConfig;


// �����������õ� Flash
static void SaveConfig(void)
{
	printf("\r\n��ʼ���� \r\n");
	STMFLASH_Write(FLASH_SAVE_ADDR, (uint32_t*)&appConfig, (sizeof(AppConfig_t) + 3) / 4);
}


static void ProcessSerialCommands(void)
{
    if (!(USART_RX_STA & 0x8000)) return;

    uint16_t len = USART_RX_STA & 0x3FFF;
    if (len >= USART_REC_LEN) len = USART_REC_LEN - 1;
    USART_RX_BUF[len] = '\0';

    // 1. ȥ���ַ���ĩβ��\r\n���������ȷƥ��
    char *p = (char *)USART_RX_BUF;
    while (*p && *p != '\r' && *p != '\n') p++;
    *p = '\0';

    int idx, r, g, b, val;

    // ================== �޲������� =======================
    if (strcmp((char*)USART_RX_BUF, "HELP") == 0)
    {
        // ��������
        printf("\r\n�����б� (�÷�: ���������ַ���������)��\r\n");
        printf("HELP                  - ��ʾ�����б�\r\n");
        printf("SHOWCFG               - ��ʾ��ǰ��������\r\n");
        printf("SETGROUPCOUNT <n>     - �������� (1~%d)\r\n", GROUP_COUNT_MAX);
        printf("SETTHRESHCOUNT <n>    - ������ֵ���� (1~%d)\r\n", ADC_THRESHOLD_COUNT_MAX);
        printf("SETCOLOR <idx> <R> <G> <B>      - ����ָ����Ļ�ɫ (���, R, G, B)\r\n");
        printf("SETTHRESH <idx> <val>           - ����ָ����ŵ�ADC��ֵ\r\n");
        printf("SETTCOLOR <idx> <R> <G> <B>     - ����ָ����ֵ����ɫ\r\n");
        printf("ADDGROUP <R> <G> <B>            - ������鲢ָ����ɫ\r\n");
        printf("DELGROUP <idx>                  - ɾ��ָ����\r\n");
        printf("ADDTHRESH <val> <R> <G> <B>     - �������ֵ������ɫ\r\n");
        printf("DELTHRESH <idx>                 - ɾ��ָ����ֵ\r\n");
				printf("SETDELAY <ms>         - ���ú�������ʱʱ�䣨��λms��0~1000��\r\n");
				printf("SETLEDCOUNT <n>       - ����LED�������� (1~2100)\r\n");
        printf("\r\n˵�������/��ֵ��Ŵ�0��ʼ������\r\n");
    }
    else if (strcmp((char*)USART_RX_BUF, "SHOWCFG") == 0)
    {
        // ��ʾ��������
        printf("\r\n------ ��ǰ���� ------\r\n");
        printf("ʵ������ groupCount=%d��ʵ����ֵ�� thresholdCount=%d\r\n", appConfig.groupCount, appConfig.thresholdCount);
        printf("�����ɫ:\r\n");
        for (int i = 0; i < appConfig.groupCount; i++) 
        {
            printf("  G%d: R=%d G=%d B=%d\r\n", i,
                appConfig.baseColors[i][0],
                appConfig.baseColors[i][1],
                appConfig.baseColors[i][2]);
        }
        printf("\r\n��ADC��ֵ����ɫ:\r\n");
        for (int i = 0; i < appConfig.thresholdCount; i++) 
        {
            printf("  T%d: Threshold=%d, Color=R%d G%d B%d\r\n", i,
                appConfig.adcThresholds[i],
                appConfig.thresholdColors[i][0],
                appConfig.thresholdColors[i][1],
                appConfig.thresholdColors[i][2]);
        }
				printf("\r\n��ǰ��������ʱ����: %d ms\r\n", appConfig.breathDelayMs);
				printf("\r\n��ǰLED����: %d\r\n", appConfig.ledCount);
        printf("---------------------\r\n");
    }

    // ================== ���������� =======================
    else if (sscanf((char*)USART_RX_BUF, "SETGROUPCOUNT %d", &val) == 1)
    {
        if (val >= 1 && val <= GROUP_COUNT_MAX) {
            appConfig.groupCount = val;
            SaveConfig();
            printf("\r\n����������Ϊ %d\r\n", val);
        } else {
            printf("\r\n������Χ: 1~%d\r\n", GROUP_COUNT_MAX);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "SETTHRESHCOUNT %d", &val) == 1)
    {
        if (val >= 1 && val <= ADC_THRESHOLD_COUNT_MAX) {
            appConfig.thresholdCount = val;
            SaveConfig();
            printf("\r\n��������ֵ��Ϊ %d\r\n", val);
        } else {
            printf("\r\n��ֵ����Χ: 1~%d\r\n", ADC_THRESHOLD_COUNT_MAX);
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
            printf("\r\n�� %d ��ɫ����Ϊ: R=%d G=%d B=%d\r\n", idx, r, g, b);
        } else {
            printf("\r\n���������� 0~%d��RGB 0~255\r\n", appConfig.groupCount - 1);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "SETTHRESH %d %d", &idx, &val) == 2)
    {
        if (idx >= 0 && idx < appConfig.thresholdCount) {
            appConfig.adcThresholds[idx] = (uint16_t)val;
            SaveConfig();
            printf("\r\n��ֵ %d ����Ϊ %d\r\n", idx, val);
        } else {
            printf("\r\n��ŷ�Χ 0~%d\r\n", appConfig.thresholdCount - 1);
        }
    }
    else if (sscanf((char*)USART_RX_BUF, "SETTCOLOR %d %d %d %d", &idx, &r, &g, &b) == 4)
    {
        if (idx >= 0 && idx < appConfig.thresholdCount && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
            appConfig.thresholdColors[idx][0] = r;
            appConfig.thresholdColors[idx][1] = g;
            appConfig.thresholdColors[idx][2] = b;
            SaveConfig();
            printf("\r\n��ֵ %d ��ɫ����Ϊ: R=%d G=%d B=%d\r\n", idx, r, g, b);
        } else {
            printf("\r\n�÷�: SETTCOLOR 0~%d R G B\r\n", appConfig.thresholdCount - 1);
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
            printf("\r\n��ɾ���� %d, ��ǰ����=%d\r\n", idx, appConfig.groupCount);
        }
        else
            printf("\r\n���ŷ�Χ 0~%d\r\n", appConfig.groupCount-1);
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
            printf("\r\n��ɾ����ֵ %d, ��ǰ��ֵ��=%d\r\n", idx, appConfig.thresholdCount);
        }
        else
            printf("\r\n��ֵ��ŷ�Χ 0~%d\r\n", appConfig.thresholdCount-1);
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
            printf("\r\n������� %d: R=%d G=%d B=%d\r\n", appConfig.groupCount-1, r, g, b);
        }
        else
            printf("\r\n�����Ѵ�����=%d\r\n", GROUP_COUNT_MAX);
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
            printf("\r\n�������ֵ %d: ��ֵ=%d ��ɫ=R%d G%d B%d\r\n",
                appConfig.thresholdCount-1, val, r, g, b);
        }
        else
            printf("\r\n��ֵ���Ѵ�����=%d\r\n", ADC_THRESHOLD_COUNT_MAX);
    }
		
		else if (sscanf((char*)USART_RX_BUF, "SETDELAY %d", &val) == 1)
		{
				if (val >= 0 && val <= 1000) { // ����Χ����
						appConfig.breathDelayMs = val;
						SaveConfig();
						printf("\r\n�����ú�����ʱΪ %d ms\r\n", val);
				} else {
						printf("\r\n������ʱ���÷�Χ: 0~1000 ms\r\n");
				}
		}
		else if (sscanf((char*)USART_RX_BUF, "SETLEDCOUNT %d", &val) == 1)
		{
				if (val >= 1 && val <= 2100) { // ����Ӳ�������趨���֧��
						appConfig.ledCount = val;
					ws2812_clear(appConfig.ledCount);
						SaveConfig();
						printf("\r\n������LED����Ϊ %d\r\n", val);
				} else {
						printf("\r\nLED�������÷�Χ: 1~2100\r\n");
				}
		}
    // ================== ����δ֪���� ======================
    else
    {
        printf("\r\nδ֪����뷢�� HELP �鿴֧�ֵ������б�\r\n");
    }

    // ������뻺����
    memset(USART_RX_BUF, 0, 10);
    USART_RX_STA = 0;
}



static void ShowConfig(void)
{
        printf("\r\n------ Current Config ------\r\n");
        printf("ʵ������ groupCount=%d��ʵ����ֵ�� thresholdCount=%d\r\n", appConfig.groupCount, appConfig.thresholdCount);
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
								printf("\r\n��ǰ��������ʱ����: %d ms\r\n", appConfig.breathDelayMs);
				printf("\r\n��ǰLED����: %d\r\n", appConfig.ledCount);
        printf("---------------------------\r\n");
    
}



// ������
static void LoadConfig(void)
{
    STMFLASH_Read(FLASH_SAVE_ADDR, (uint32_t*)&appConfig, (sizeof(AppConfig_t) + 3) / 4);

    // ����Ƿ�ȫΪ0xFF�����״�д��ų�ʼ��
    int allff = 1;
    uint8_t *p = (uint8_t*)&appConfig;
    for (int i = 0; i < sizeof(AppConfig_t); ++i) {
        if (p[i] != 0xFF) { allff = 0; break; }
    }
    if (allff) {
        // ֻ�ڵ�һ��ȫ��ʱ��ʼ��
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
				appConfig.ledCount = 2100;     // Ĭ��2100
				appConfig.breathDelayMs = 0; // Ĭ��20ms��������ʱ
        appConfig.groupCount = GROUP_COUNT_MAX;
        appConfig.thresholdCount = ADC_THRESHOLD_COUNT_MAX;
        SaveConfig();
    }

    // ���״Σ�ֻ����Ч��Χ�ݴ��������ٳ�ʼ��
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
    static int lastMode = -1;    // -1:δ����, 0:����, 1:��ֵ

    while (1)
    {
        ProcessSerialCommands();

        uint16_t adcValue = 300; // TODO: �������ADC����ֵ
        uint8_t curColor[3] = {0};
        int found = 0;
        int curThresholdIdx = -1;

        // ֻ�ж�ʵ����ֵ����
        for (int i = appConfig.thresholdCount - 1; i >= 0; --i) {
            if (adcValue >= appConfig.adcThresholds[i]) {
                for (int k = 0; k < 3; k++)
                    curColor[k] = appConfig.thresholdColors[i][k];
                found = 1;
                curThresholdIdx = i;
                break;
            }
        }

        // ����Ƿ��л�ģʽ
        int curMode = found ? 1 : 0;    // 1:��ֵ, 0:����
        if (curMode != lastMode) {
            if (curMode == 0) {  // �лغ�����
                step = 0;
                dir = 1;
                // groupIndex �ɱ���ԭֵ��Ҳ�����ã����������
            }
            lastMode = curMode;
        }

        // ������ʾɫ
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
                // ÿ�л�����һ�飬�������´������
                step = 0;
                dir = 1;
            }
            delay_ms(appConfig.breathDelayMs);  // ������ˢ�����ʣ����е��ڣ�
        } else {
            step = BREATH_STEPS;
            dir = 1;
//            delay_ms(60);  // �̶�����ʾʱˢ�����ʣ����е��ڣ�
        }

        // ֻ����ֵ�仯/ģʽ�仯ʱ��ӡһ��
        if (curThresholdIdx != lastThresholdIdx) {
            if (curThresholdIdx >= 0)
                printf("��ǰ���ڵ�%d����ֵ: ��ֵ=%d, ADC=%d\r\n", curThresholdIdx, appConfig.adcThresholds[curThresholdIdx], adcValue);
            else
                printf("��ǰΪ������ģʽ, ADC=%d\r\n", adcValue);
            lastThresholdIdx = curThresholdIdx;
        }
    }
}
