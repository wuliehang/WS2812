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
    uint8_t baseColors[GROUP_COUNT][3];            // 呼吸灯分组颜色
    uint16_t adcThresholds[ADC_THRESHOLD_COUNT];   // ADC阈值
    uint8_t thresholdColors[ADC_THRESHOLD_COUNT][3]; // 阈值对应颜色
} AppConfig_t;

static AppConfig_t appConfig;

// 呼吸状态
int groupIndex = 0;
uint8_t step = 0;
int8_t dir = 1;

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
    if (len >= USART_REC_LEN) len = USART_REC_LEN-1;
    USART_RX_BUF[len] = '\0';

    int idx, r, g, b, val;

    // 修改某组呼吸基色 SETCOLOR <group> <R> <G> <B>
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
    // 设置ADC阈值 SETTHRESH <idx> <value>
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
    // 设置阈值颜色 SETTCOLOR <idx> <R> <G> <B>
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

	 // 串口命令：SHOWCFG
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



// 读配置
static void LoadConfig(void)
{
    STMFLASH_Read(FLASH_SAVE_ADDR, (uint32_t*)&appConfig, (sizeof(AppConfig_t) + 3) / 4);
    // baseColors默认填充
    for (int i=0; i<GROUP_COUNT; ++i) {
        if (appConfig.baseColors[i][0]==0xFF && appConfig.baseColors[i][1]==0xFF && appConfig.baseColors[i][2]==0xFF) {
            appConfig.baseColors[i][0]=150;
            appConfig.baseColors[i][1]=150;
            appConfig.baseColors[i][2]=150;
        }
    }
    // 阈值、颜色也可做类似判断并初始化
    for (int i=0; i<ADC_THRESHOLD_COUNT; ++i) {
        if (appConfig.adcThresholds[i]==0xFFFF)
            appConfig.adcThresholds[i]=200*(i+1); // 默认填一些值
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
	 int curThresholdIdx = -1; // 当前处于哪个阈值，-1代表呼吸灯

        uint16_t adcValue = 2000; // 你自己写的ADC采集函数
        uint8_t curColor[3];
        int found = 0;

        // 优先级从高到低判断
        for (int i = ADC_THRESHOLD_COUNT - 1; i >= 0; --i) {
            if (adcValue >= appConfig.adcThresholds[i]) {
                for(int k=0;k<3;k++)
                    curColor[k] = appConfig.thresholdColors[i][k];
                found = 1;
							 curThresholdIdx = i; // 记录命中第几个阈值
                break;
            }
        }
        if (!found) {
            // 呼吸模式
            curColor[0] = (appConfig.baseColors[groupIndex][0] * step) / BREATH_STEPS;
            curColor[1] = (appConfig.baseColors[groupIndex][1] * step) / BREATH_STEPS;
            curColor[2] = (appConfig.baseColors[groupIndex][2] * step) / BREATH_STEPS;
        }

        for (uint16_t i = 1; i <= LED_COUNT; i++)
            ws2812_setPixelColor(curColor, i);
        ws2812_show(LED_COUNT);

        // 呼吸灯 step 只在呼吸模式下变动
        if (!found) {
            step += dir;
            if (step == BREATH_STEPS || step == 0) {
                dir = -dir;
                if (step == 0) {
                    groupIndex = (groupIndex + 1) % GROUP_COUNT;
                }
            }
        } else {
            // 非呼吸色，step固定为最大亮
            step = BREATH_STEPS;
            dir = 1;
        }
				
//				     // 打印当前处于哪个阈值
//        if (curThresholdIdx >= 0)
//            printf("当前处于第%d个阈值（索引从0）: 阈值=%d, ADC=%d\r\n", curThresholdIdx, appConfig.adcThresholds[curThresholdIdx], adcValue);
//        else
//            printf("当前为呼吸灯模式, ADC=%d\r\n", adcValue);
//				
//				delay_ms(800);
    }
}




//// Flash 存储起始地址，任选一个空闲扇区
//#define FLASH_SAVE_ADDR   ADDR_FLASH_SECTOR_11
//#define GROUP_COUNT   10      // 最多支持 10 组颜色


//// 内存中保存的多组基色 (R,G,B)
//static uint8_t baseColors[GROUP_COUNT][3];

//// 启动时从 Flash 载入所有组
//static void LoadColors(void)
//{
//	u32 tmp[GROUP_COUNT];
//	STMFLASH_Read(FLASH_SAVE_ADDR, tmp, GROUP_COUNT);
//	for (int g = 0; g < GROUP_COUNT; g++) 
//	{
//		if (tmp[g] == 0xFFFFFFFF) 
//		{
//			// Flash 默认是全 1，表示未写过，使用白色
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

//// 将内存中所有组一次性写回 Flash（重写全部 GROUP_COUNT 个 word）
//static void SaveColors(void)
//{
//	u32 tmp[GROUP_COUNT];
//	for (int g = 0; g < GROUP_COUNT; g++) 
//	{
//		tmp[g] = (baseColors[g][0] << 16)| (baseColors[g][1] <<  8) | baseColors[g][2];
//	}
//	STMFLASH_Write(FLASH_SAVE_ADDR, tmp, GROUP_COUNT);
//}


///*—— 串口命令处理（被 main 循环调用） ——*/
//static void ProcessSerialCommands(void)
//{
//	if (USART_RX_STA & 0x8000) 
//	{
//			uint16_t len = USART_RX_STA & 0x3FFF;
//			// 在尾部加 '\0'
//			if (len >= USART_REC_LEN) len = USART_REC_LEN-1;
//			USART_RX_BUF[len] = '\0';

//			// 解析命令
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

//// 颜色表 (R, G, B)
//const uint8_t colorTable[][3] = {
//    {255,   0,   0},  // 红
//    {  0, 255,   0},  // 绿
//    {  0,   0, 255},  // 蓝
//    {255, 255,   0},  // 黄
//    {255,   0, 255},  // 紫
//    {  0, 255, 255},  // 青
//};
//#define COLOR_COUNT  (sizeof(colorTable)/sizeof(colorTable[0]))
//#define LED_COUNT    2100
//#define BREATH_STEPS   50    // 呼吸级数
//int groupIndex = 0,j;

//uint8_t step     = 0;     // 当前级数 [0..BREATH_STEPS]
//int8_t dir       = 1;     // 1=变亮, -1=变暗
//uint8_t colorIdx = 0;

//// 工作缓冲：一颗灯的颜色
//uint8_t rgb[3];

//int main(void)
//{
//	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
//	delay_init(168);
//	ws2812_init();
//	uart_init(115200);
//	printf("呼吸灯(21)启动，8级分辨率\n");
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
//		//		// —— 更新呼吸级数 —— 
//		step += dir;
//		if (step == BREATH_STEPS || step == 0) 
//		{
//			dir = -dir;                         // 翻转方向
//			if (step == 0) 
//			{
//				groupIndex = (groupIndex + 1) % GROUP_COUNT;
//				printf("切换到组 %d\r\n", groupIndex);
//			}
//		}

//		// 计算渐变色
//		rgb[0] = (baseColors[groupIndex][0] * step) / BREATH_STEPS;
//		rgb[1] = (baseColors[groupIndex][1] * step) / BREATH_STEPS;
//		rgb[2] = (baseColors[groupIndex][2] * step) / BREATH_STEPS;

//		for (uint16_t i = 1; i <= LED_COUNT; i++)
//		ws2812_setPixelColor(rgb, i);
//		ws2812_show(LED_COUNT);
//	}
//}

