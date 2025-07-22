#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "pwm.h"
#include "ws2812.h"
#include "stmflash.h" 

#define GROUP_COUNT         10
#define ADC_THRESHOLD_COUNT 5
#define LED_COUNT           2100
#define BREATH_STEPS        50

#define FLASH_SAVE_ADDR     ADDR_FLASH_SECTOR_11

typedef struct __attribute__((packed))  {
    uint8_t baseColors[GROUP_COUNT][3];            // �����Ʒ�����ɫ
    uint16_t adcThresholds[ADC_THRESHOLD_COUNT];   // ADC��ֵ
    uint8_t thresholdColors[ADC_THRESHOLD_COUNT][3]; // ��ֵ��Ӧ��ɫ
} AppConfig_t;

static AppConfig_t appConfig;

// ����״̬
int groupIndex = 0;
uint8_t step = 0;
int8_t dir = 1;

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
    if (len >= USART_REC_LEN) len = USART_REC_LEN-1;
    USART_RX_BUF[len] = '\0';

    int idx, r, g, b, val;

    // �޸�ĳ�������ɫ SETCOLOR <group> <R> <G> <B>
    if (sscanf((char*)USART_RX_BUF, "SETCOLOR %d %d %d %d", &idx, &r, &g, &b) == 4)
    {
        if (idx>=0 && idx<GROUP_COUNT && r>=0 && r<=255 && g>=0 && g<=255 && b>=0 && b<=255)
        {
            appConfig.baseColors[idx][0] = r;
            appConfig.baseColors[idx][1] = g;
            appConfig.baseColors[idx][2] = b;
            SaveConfig();
            printf("\r\nGroup %d -> R=%d G=%d B=%d\r\n", idx, r, g, b);
        }
        else
            printf("\r\nErr: group 0~%d, RGB 0~255\r\n", GROUP_COUNT-1);
    }
    // ����ADC��ֵ SETTHRESH <idx> <value>
    else if (sscanf((char*)USART_RX_BUF, "SETTHRESH %d %d", &idx, &val) == 2)
    {
        if (idx>=0 && idx<ADC_THRESHOLD_COUNT)
        {
            appConfig.adcThresholds[idx] = (uint16_t)val;
            SaveConfig();
            printf("\r\nThreshold %d set to %d\r\n", idx, val);
        }
        else
            printf("\r\nIdx 0~4\r\n");
    }
    // ������ֵ��ɫ SETTCOLOR <idx> <R> <G> <B>
    else if (sscanf((char*)USART_RX_BUF, "SETTCOLOR %d %d %d %d", &idx, &r, &g, &b) == 4)
    {
        if (idx>=0 && idx<ADC_THRESHOLD_COUNT && r>=0 && r<=255 && g>=0 && g<=255 && b>=0 && b<=255)
        {
            appConfig.thresholdColors[idx][0] = r;
            appConfig.thresholdColors[idx][1] = g;
            appConfig.thresholdColors[idx][2] = b;
            SaveConfig();
            printf("\r\nTColor %d -> R=%d G=%d B=%d\r\n", idx, r, g, b);
        }
        else
            printf("\r\nUsage: SETTCOLOR 0~4 R G B\r\n");
			}

	 // �������SHOWCFG
	else if (sscanf((char*)USART_RX_BUF, "SHOWCFG") == 0)
	{
			printf("\r\n------ Current Config ------\r\n");
			printf("Breath Base Colors (Group):\r\n");
			for (int i = 0; i < GROUP_COUNT; i++) 
{
					printf("  G%d: R=%d G=%d B=%d\r\n", i,
							appConfig.baseColors[i][0],
							appConfig.baseColors[i][1],
							appConfig.baseColors[i][2]);
			}

			printf("\r\nADC Thresholds & Colors:\r\n");
			for (int i = 0; i < ADC_THRESHOLD_COUNT; i++) 
			{
					printf("  T%d: Threshold=%d, Color=R%d G%d B%d\r\n", i,
							appConfig.adcThresholds[i],
							appConfig.thresholdColors[i][0],
							appConfig.thresholdColors[i][1],
							appConfig.thresholdColors[i][2]);
			}
			printf("---------------------------\r\n");
	}
	USART_RX_STA = 0;
}

static void ShowConfig(void)
{
	printf("\r\n------ Current Config ------\r\n");
	printf("Breath Base Colors (Group):\r\n");
	for (int i = 0; i < GROUP_COUNT; i++) 
	{
		printf("  G%d: R=%d G=%d B=%d\r\n", i,
		appConfig.baseColors[i][0],
		appConfig.baseColors[i][1],
		appConfig.baseColors[i][2]);
	}
	printf("\r\nADC Thresholds & Colors:\r\n");
	for (int i = 0; i < ADC_THRESHOLD_COUNT; i++) 
	{
		printf("  T%d: Threshold=%d, Color=R%d G%d B%d\r\n", i,
		appConfig.adcThresholds[i],
		appConfig.thresholdColors[i][0],
		appConfig.thresholdColors[i][1],
		appConfig.thresholdColors[i][2]);
	}
	printf("---------------------------\r\n");
}



// ������
static void LoadConfig(void)
{
    STMFLASH_Read(FLASH_SAVE_ADDR, (uint32_t*)&appConfig, (sizeof(AppConfig_t) + 3) / 4);
    // baseColorsĬ�����
    for (int i=0; i<GROUP_COUNT; ++i) {
        if (appConfig.baseColors[i][0]==0xFF && appConfig.baseColors[i][1]==0xFF && appConfig.baseColors[i][2]==0xFF) {
            appConfig.baseColors[i][0]=150;
            appConfig.baseColors[i][1]=150;
            appConfig.baseColors[i][2]=150;
        }
    }
    // ��ֵ����ɫҲ���������жϲ���ʼ��
    for (int i=0; i<ADC_THRESHOLD_COUNT; ++i) {
        if (appConfig.adcThresholds[i]==0xFFFF)
            appConfig.adcThresholds[i]=200*(i+1); // Ĭ����һЩֵ
        if (appConfig.thresholdColors[i][0]==0xFF && appConfig.thresholdColors[i][1]==0xFF && appConfig.thresholdColors[i][2]==0xFF) {
            uint8_t colorPreset[5][3] = {
                {255, 0, 0}, {0,255,0}, {0,0,255}, {255,255,0}, {255,0,255}
            };
            for(int k=0;k<3;k++) appConfig.thresholdColors[i][k]=colorPreset[i][k];
        }
    }
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);
    ws2812_init();
    uart_init(115200);

    LoadConfig();
    ws2812_clear(LED_COUNT);
		ShowConfig();
    while (1)
    {
        ProcessSerialCommands();
	 int curThresholdIdx = -1; // ��ǰ�����ĸ���ֵ��-1���������

        uint16_t adcValue = 2000; // ���Լ�д��ADC�ɼ�����
        uint8_t curColor[3];
        int found = 0;

        // ���ȼ��Ӹߵ����ж�
        for (int i = ADC_THRESHOLD_COUNT - 1; i >= 0; --i) {
            if (adcValue >= appConfig.adcThresholds[i]) {
                for(int k=0;k<3;k++)
                    curColor[k] = appConfig.thresholdColors[i][k];
                found = 1;
							 curThresholdIdx = i; // ��¼���еڼ�����ֵ
                break;
            }
        }
        if (!found) {
            // ����ģʽ
            curColor[0] = (appConfig.baseColors[groupIndex][0] * step) / BREATH_STEPS;
            curColor[1] = (appConfig.baseColors[groupIndex][1] * step) / BREATH_STEPS;
            curColor[2] = (appConfig.baseColors[groupIndex][2] * step) / BREATH_STEPS;
        }

        for (uint16_t i = 1; i <= LED_COUNT; i++)
            ws2812_setPixelColor(curColor, i);
        ws2812_show(LED_COUNT);

        // ������ step ֻ�ں���ģʽ�±䶯
        if (!found) {
            step += dir;
            if (step == BREATH_STEPS || step == 0) {
                dir = -dir;
                if (step == 0) {
                    groupIndex = (groupIndex + 1) % GROUP_COUNT;
                }
            }
        } else {
            // �Ǻ���ɫ��step�̶�Ϊ�����
            step = BREATH_STEPS;
            dir = 1;
        }
				
//				     // ��ӡ��ǰ�����ĸ���ֵ
//        if (curThresholdIdx >= 0)
//            printf("��ǰ���ڵ�%d����ֵ��������0��: ��ֵ=%d, ADC=%d\r\n", curThresholdIdx, appConfig.adcThresholds[curThresholdIdx], adcValue);
//        else
//            printf("��ǰΪ������ģʽ, ADC=%d\r\n", adcValue);
//				
//				delay_ms(800);
    }
}




//// Flash �洢��ʼ��ַ����ѡһ����������
//#define FLASH_SAVE_ADDR   ADDR_FLASH_SECTOR_11
//#define GROUP_COUNT   10      // ���֧�� 10 ����ɫ


//// �ڴ��б���Ķ����ɫ (R,G,B)
//static uint8_t baseColors[GROUP_COUNT][3];

//// ����ʱ�� Flash ����������
//static void LoadColors(void)
//{
//	u32 tmp[GROUP_COUNT];
//	STMFLASH_Read(FLASH_SAVE_ADDR, tmp, GROUP_COUNT);
//	for (int g = 0; g < GROUP_COUNT; g++) 
//	{
//		if (tmp[g] == 0xFFFFFFFF) 
//		{
//			// Flash Ĭ����ȫ 1����ʾδд����ʹ�ð�ɫ
//			baseColors[g][0] = 150;
//			baseColors[g][1] = 150;
//			baseColors[g][2] = 150;
//		} 
//		else 
//		{
//			baseColors[g][0] = (tmp[g] >> 16) & 0xFF;
//			baseColors[g][1] = (tmp[g] >>  8) & 0xFF;
//			baseColors[g][2] = (tmp[g]      ) & 0xFF;
//		}
//	}
//}

//// ���ڴ���������һ����д�� Flash����дȫ�� GROUP_COUNT �� word��
//static void SaveColors(void)
//{
//	u32 tmp[GROUP_COUNT];
//	for (int g = 0; g < GROUP_COUNT; g++) 
//	{
//		tmp[g] = (baseColors[g][0] << 16)| (baseColors[g][1] <<  8) | baseColors[g][2];
//	}
//	STMFLASH_Write(FLASH_SAVE_ADDR, tmp, GROUP_COUNT);
//}


///*���� ����������� main ѭ�����ã� ����*/
//static void ProcessSerialCommands(void)
//{
//	if (USART_RX_STA & 0x8000) 
//	{
//			uint16_t len = USART_RX_STA & 0x3FFF;
//			// ��β���� '\0'
//			if (len >= USART_REC_LEN) len = USART_REC_LEN-1;
//			USART_RX_BUF[len] = '\0';

//			// ��������
//			int group, r, g, b;
//			if (sscanf((char*)USART_RX_BUF, "SETCOLOR %d %d %d %d", &group, &r, &g, &b) == 4)
//			{
//				if (group>=0 && group<GROUP_COUNT
//				 && r>=0 && r<=255
//				 && g>=0 && g<=255
//				 && b>=0 && b<=255)
//				{
//					baseColors[group][0] = (uint8_t)r;
//					baseColors[group][1] = (uint8_t)g;
//					baseColors[group][2] = (uint8_t)b;
//					SaveColors();
//					printf("\r\nGroup %d -> R=%d G=%d B=%d\r\n",group, r, g, b);
//				} 
//				else 
//				{
//					printf("\r\nErr: group 0~%d, RGB 0~255\r\n",	GROUP_COUNT-1);
//				}
//			} 
//			else 
//			{
//				printf("\r\nUsage: SETCOLOR <group> <R> <G> <B>\r\n");
//			}
//			USART_RX_STA = 0;
//	}
//}

//// ��ɫ�� (R, G, B)
//const uint8_t colorTable[][3] = {
//    {255,   0,   0},  // ��
//    {  0, 255,   0},  // ��
//    {  0,   0, 255},  // ��
//    {255, 255,   0},  // ��
//    {255,   0, 255},  // ��
//    {  0, 255, 255},  // ��
//};
//#define COLOR_COUNT  (sizeof(colorTable)/sizeof(colorTable[0]))
//#define LED_COUNT    2100
//#define BREATH_STEPS   50    // ��������
//int groupIndex = 0,j;

//uint8_t step     = 0;     // ��ǰ���� [0..BREATH_STEPS]
//int8_t dir       = 1;     // 1=����, -1=�䰵
//uint8_t colorIdx = 0;

//// �������壺һ�ŵƵ���ɫ
//uint8_t rgb[3];

//int main(void)
//{
//	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
//	delay_init(168);
//	ws2812_init();
//	uart_init(115200);
//	printf("������(21)������8���ֱ���\n");
//	LoadColors();
//	printf("Loaded colors:\r\n");
//	for(int i=0;i<GROUP_COUNT;i++)
//	{
//		printf(" G%d=%d,%d,%d\r\n",i, baseColors[i][0], baseColors[i][1], baseColors[i][2]);
//	}
//	ws2812_clear(LED_COUNT);
//	
//	while (1)
//	{	
//		ProcessSerialCommands();
//		//		// ���� ���º������� ���� 
//		step += dir;
//		if (step == BREATH_STEPS || step == 0) 
//		{
//			dir = -dir;                         // ��ת����
//			if (step == 0) 
//			{
//				groupIndex = (groupIndex + 1) % GROUP_COUNT;
//				printf("�л����� %d\r\n", groupIndex);
//			}
//		}

//		// ���㽥��ɫ
//		rgb[0] = (baseColors[groupIndex][0] * step) / BREATH_STEPS;
//		rgb[1] = (baseColors[groupIndex][1] * step) / BREATH_STEPS;
//		rgb[2] = (baseColors[groupIndex][2] * step) / BREATH_STEPS;

//		for (uint16_t i = 1; i <= LED_COUNT; i++)
//		ws2812_setPixelColor(rgb, i);
//		ws2812_show(LED_COUNT);
//	}
//}

