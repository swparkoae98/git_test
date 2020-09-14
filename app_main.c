/******************************************************************************
 * NAW Board
 * Copyright by UDWorks, Incoporated. All Rights Reserved.
 * Modified by S.W.Park
 *---------------------------------------------------------------------------*/
 /**
 * @file    app_main.c
 * @brief
 */
/*****************************************************************************/

/*----------------------------------------------------------------------------
 Defines referenced header files
-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <signal.h>

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <mysql.h>

#include "Global_Main.h"

#include "ti_vsys.h"

#include "app_comm.h"
#include "app_ctrl.h"
#include "app_dev.h"
#include "app_main.h"
#include "app_key.h"

#include <dev_gpio.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "Function.h"
#include "EquipmentT.h"
#include "RS232T.h"
#include "Tel_SMST.h"
#include "BroadcastT.h"
#include "MainMICT.h"
#include "StatusProcessT.h"
#include "ButtonT.h"
#include "SerialT.h"
#include "PSTN_T.h"

#include "app_gui.h"
#include "crypt.h"
#include "klog.h"

/*----------------------------------------------------------------------------
 Definitions and macro
-----------------------------------------------------------------------------*/
//#define DEF_AUTO_REBOOT_TEST
#define WATCHDOG_TIME_INTERVAL	110
#define IP_UDP_PORT	13503
#define WATER_WINDOW_CNT 5
#define DEF_FORK_PROCESS_USE

/*----------------------------------------------------------------------------
 Declares variables
-----------------------------------------------------------------------------*/
app_cfg_t cfg_obj;
app_cfg_t *iapp = &cfg_obj;

app_cfg_t *app_cfg;

app_thr_obj _objM2mIpRcvThread;
app_thr_obj *_tobjM2mIpRcvThread = &_objM2mIpRcvThread;  

app_thr_obj _objM2mIpSndThread;
app_thr_obj *_tobjM2mIpSndThread = &_objM2mIpSndThread;  

app_thr_obj _objIpStatusReportThread;
app_thr_obj *_tobjIpStatusReportThread = &_objIpStatusReportThread;  

app_thr_obj _objHWwatchdogThread;
app_thr_obj *_tobjHWwatchdogThread = &_objHWwatchdogThread;  

int IpStsID;

/*----------------------------------------------------------------------------
 Declares a function prototype
-----------------------------------------------------------------------------*/
pthread_t RS232T, EquipmentT;						// thread variables
thdData rs232data, equipmentdata;			// structs to be passed to threads
	
pthread_t telsmsThread;
thdData telsmsthdata;

pthread_t workThread;
thdData workthdata;

pthread_t adcTestThread;
thdData adcTetstthdata;

int CFUN_flag = -1;
int g_play_time=0;
int g_live_client_sockfd = -1;
char g_str_guid[32];
int g_try_count=0;
int g_rec_pre_time = 0;
int g_t_status=3;
int g_ip_sts_flag = 0;
int g_pri_stop_flag = 0;
int g_ip_route_id = -1;
int g_libimg_db_size = 0;
int g_lib_index_id = 0;
int g_log_data_send_flag = 0;
int g_route_chg_flag = 0;

#ifdef DEF_HW_WATCHDOG_USE
int g_hwwatch_pid=-1;
int g_pwr_hw_ver = 0;
#endif
extern int ppp_conn_flag;
extern unsigned char g_modem_comp_id;
extern char g_gateway_ip[20];
extern unsigned short g_ip_report_time;
extern unsigned char g_lte_status;
extern unsigned char g_lte_log;
extern unsigned char g_lte_alarm;
extern unsigned char g_lte_warning;
extern unsigned char g_lte_command;
extern int g_battery_alarm;
extern int g_battery_warning;
extern int g_AC_power_alarm;
int g_modem_svc_alarm = -1;
unsigned char g_rcv_flag_from_ip_server = 0;
extern int g_Ext_AMP_alarm;
extern int g_Speaker_alarm;
extern int g_DIF_alarm;
extern Water_Level_Config_Data water_level_data;
extern Door_Check_Config_Data door_check_data;
extern int g_restart_flag;
extern int g_cdma_reset_flag;
extern int g_ppp_run_count;

#ifdef USE_FILE_LOG
#ifdef DEF_MQUEUE_LOG_USE
extern int mqueue_log_receive_thread(void *ptr);
#else
pthread_mutex_t g_fi_mutex;
#endif
extern int g_log_print_flag;
void log_data_file_setting();
#endif

#ifdef DEF_RESERVED_BROADCAST_USE
extern Rsv_Setting_Data g_rsv_read_data;
#endif

#ifdef DEF_MQUEUE_USE
int pipe_fd[2];
extern int mqueue_aplay_thread(void *ptr);
extern int mqueue_aplay_run_thread(void *ptr);
#endif

#ifdef DEF_AUTO_REBOOT_TEST
int Auto_reboot_proc();
#endif

#ifndef USE_SUJAWON_DEF
int socket_client_main();
int socket_server_main();
void Recieve_Command_from_IP_Server_process(int client_sockfd, char *buf);
int socket_m2m_client_main();
void Read_IP_File_List(char *s_sdata);
void M2mIpReg_Send_Proc();
#endif

#ifndef ADMIN_CHIME_TIME_DEF
#ifdef HAENAM_CHAIM
extern int HaeNAM_ChimeBell_PlayTime_OAEPRO;
#ifdef HAENAM_LEEJANG_CHAIM
extern int HaeNAM_ChimeBell_PlayTime_LEEJANG;
#endif
#ifdef HAENAM_JAENAN_CHAIM
extern int HaeNAM_ChimeBell_PlayTime_JAENAN;
#endif
#else
extern int ChimeBell_PlayTime_OAEPRO;
#endif
#endif

int udp_client_report(char buf[TOT_IP_SEND_MSG_LEN]);
void ack_reponse_to_server_by_socket(int client_sockfd, char *ack_buf);
void ack_reponse_to_server_by_socket_24(int client_sockfd, char *ack_buf);
void req_file_broadcast_msgq(char *file_buf);
void req_live_broadcast_msgq(char *file_buf);
void req_tts_broadcast_msgq(char *file_buf);
void req_rec_live_broadcast_msgq(char *file_buf);
int get_g_play_time();
int get_g_rec_pre_time();
int get_CFUN_flag();
void set_CFUN_flag(int loc_flag);
void Chime_Time_setting();
void Modem_Initial_Processing();
void IP_Broadcast_Thread_Generation(app_thr_obj *tObj);
void Modem_ip_link_setup();
void udp_client_report_data_save_to_file(char *buf);
void water_level_check_proc();
void water_ment_processing(app_thr_obj *_tloc_objWaterLevel_Ment_playThread);

struct server_braod_mx_data* get_g_srv_broad_mx();
void udp_message_data_send_proc(char *log_data, int cmd_id);
void file_list_ack_reponse_to_server_by_socket(int client_sockfd, char *ack_buf,int len);
void time_delay_function();
void stop_currently_runing_service(int,int);
#ifndef DEF_MODEM_RESTART_NEW_PROC
void sigint_handler( int signo);
#endif
void req_live_broadcast_act_msgq();
int get_g_live_client_sockfd();
void set_g_live_client_sockfd(int loc_fd);
char* get_g_str_guid();
void req_siren_broadcast_msgq(char *file_buf);
int get_g_try_count();
void ip_status_report_proc(int t_status);
void socket_ip_status_report_thread_proc();
void sendServerIpStatusMsg(int messageType);
int IpStsTfunction();
int get_g_pri_stop_flag();
void set_g_pri_stop_flag(int loc_flag);
int get_system_MyIPAddress(char *ip_addr, char *nic_name);
void InitAuth(void);

#ifndef DEF_SYSTEM_TIMER_THREAD_USE
void timerHandler( int sig, siginfo_t *si, void *uc );
#else
void timerHandler( union sigval si );
#endif

int read_eth_link_check_processing();
void ip_link_check_route_setting(unsigned char id,char *gw_address, char *device_name);
int getrouteState();
int send_udp_client_report_data(char buf[TOT_IP_SEND_MSG_LEN]);
void get_alarm_n_warning_Read_from_DB();
int get_system_status_info_read_from_server(int idx);
void req_emergency_broadcast_msgq(char *file_buf);
void send_to_server_msg_Boot_start_Message();
void app_thr_wait_microsecs(unsigned int microsecs);
int vpn_config_file_checking_n_write();
int get_pid_by_process_name(char *process_name);
long get_filesize(char *name);
unsigned short server_oae_crypt();
unsigned short client_oae_crypt();
void water_level_ment_play_processing(void *ptr);
void Signal_list(void);
void sigterm_handler(int signo);
void kill_watchdog_process();
int ENT_Socket_closed_proc(void *ptr);
void modem_system_route_add(char *gw_address,char *device_name);
void modem_system_route_delete(char *gw_address,char *device_name);
void eth0_system_route_add();
void eth0_system_route_delete();

extern void setWDT_KeepAlive();
extern int readPowerBoard_DI_for_door_open(unsigned short di_id,unsigned short di_act);

#ifdef DEF_MODEM_RESTART_NEW_PROC
int g_mdm_rst_count = 0;
timer_t g_modem_s_timer;
timer_t g_wtd_s_timer;
app_thr_obj _objThreadRSTModem;
app_thr_obj *_tobjThreadRSTModem = &_objThreadRSTModem;

app_thr_obj _objThreadModemCheck_init;
app_thr_obj *_tobjThreadModemCheck_init = &_objThreadModemCheck_init;

int checking_modem_restart_count();
void rst_count_add();
void for_modem_init_timer_proc();
void start_modem_reset_n_check(int time_modem);
#ifdef DEF_DOOR_OPEN_SEND_MSG_USE   //door open SMS report processing
void door_open_check_proc();
void send_SMS_message_for_door_open_close(int on_off_flag);
#endif
extern void Modem_reset_n_pwr_off_on();
extern void startThreadTelSMS(int start);
extern void send_SMS_Message(char *send_data,int id,int who_id);
#endif

#ifdef DEF_NET_TIME_USE
#define BORA_NET	"203.248.240.140"

void net_time_setting();
int getRdateState();
#endif

#ifndef DEF_WATCHDOG_SYS_TIME_USE
void watchdog_report_thraead();
#else
app_thr_obj _objThreadWatchdog_setting;
app_thr_obj *_tobjThreadWatchdog_setting = &_objThreadWatchdog_setting;
#endif

#ifdef DEF_SYSTEM_TIMER_THREAD_USE
void setWDT_KeepAlive_handle(union sigval si);
void for_modem_init_timer_proc_handle(union sigval si);
#endif

#ifdef DEF_RESERVED_BROADCAST_USE
void reserved_file_directory_check_n_creat();
void reserved_broadcast_processing();
void Read_from_reserved_list_file();
#endif

void HW_watchdog_disable();
void HW_watchdog_enable();
void libimg_libray_size_check();

#ifdef DEF_ENTIMORE_IP_USE
int ethernet_link_connect_check();
void Entimore_data_link_resetup(int idx);
#endif

Vpn_Config_Data g_vpn_config_data;
extern char oaepro_aun_key_str[5][9];

extern void rebootSystemCDMA(int flag);
extern int getCurrentBroadcastState(void);
extern void cdma_EndCall();
extern int get_exit_boardcast_stop();
extern void Tel_connection_disconnect();
extern void set_isConnecting(int r_conn);
extern void set_isBroadcastTel(int r_broad);
extern int get_g_broadcastSubType();
extern int get_g_PSTN_exit_flag();
extern int get_g_call_end_flag();
extern void set_g_PSTN_exit_flag(int g_flag);
extern void set_g_call_end_flag(int g_flag);
extern char* get_server_ip();
extern void set_broad_file_name(char *file_path);
extern void set_g_stop_proc_check_flag(int flag);
extern int get_g_stop_proc_check_flag();
extern void init_encode_decode();
extern pid_t get_tts_child_prcessor();
extern void set_tts_child_prcessor(pid_t loc_pid);
extern void set_IP_Req_Status_Index(int s_mode);
extern void checkStateSudong();
extern void StatusProcess_init();
extern void sendBroadcastRecLIVEMsg(int mode, unsigned char value, char* filePath);
extern void sendBroadcastMsgRECLIVEPlay(int messageType, char* filePath, int groupNumber, int useAMP, int use422);
extern void sendBroadcastMsgTTSPlay(int messageType, char* filePath, int groupNumber, int useAMP, int use422);
extern void sendBroadcastMsgFilePlay(int messageType, char* filePath, int groupNumber, int useAMP, int use422);
extern void sendBroadcastLIVEMsg(int mode, int value, char* filePath);
extern void sendBroadcastTTSMsg(int mode, int value, char* filePath);
extern void sendBroadcastMsgLIVEPlay(int messageType, char* filePath, int groupNumber, int useAMP, int use422);
extern void file_read_date_time();
extern int get_g_SunCha_Mode();
extern void set_g_SunCha_Mode();
extern void startSunCha_Broadcast_Play(int start);
extern uint16_t get_g_server_ipcomm_port();
extern void setPPP();
extern void read_IP_ID_value_to_gMyCallNumber();
extern int get_g_OAEPRO_STATE_flag();
extern void read_IP_ID_value_to_gMyCallNumber_no_check();
extern uint16_t get_check422();
extern uint16_t get_checkBCDMA_VAL();
extern void new_lcd_gpio_set();
extern int get_g_lcd_version_flag();
extern void write_app_sw_ver_to_broadcast();
extern void data_buffer_save_n_send_bt80(unsigned char id, unsigned char val1, unsigned char val2);
#ifndef DEF_SYSTEM_TIMER_THREAD_USE
extern void StartTimer_sys(timer_t* timerid,void (*Func)(), long msecInterval);
#else
extern void StartTimer_sys(timer_t* timerid,void (*Func)(union sigval), long msecInterval);
#endif

extern void StopTimer_sys(timer_t timerId);
extern void StartTimer_Once(timer_t* timerid,void (*Func)(), long msecInterval);
extern void Telradin_modem_init();
extern unsigned short * get_g_loc_lo_ip();
extern void sendBroadcastReserveMsg(int mode, int value, char* filePath);
extern void sendBroadcastMsgEmergencyPlay(int messageType, char* filePath, int groupNumber, int useAMP, int use422);
extern void sendBroadcastEMERGENCYMsg(int mode, int value, char* filePath);
extern void mtom_modem_init();
extern void cdma_EndCall_direct(void);
extern void modem_service_alarm_ON_OFF_report(int on_off_flag);
extern void send_FileMsg_to_BroadcastThread(int messageType, char* filePath, int groupNumber, int useAMP, int use422,int broadcastType);
extern void sendBroadcastMsgFilePlay(int messageType, char* filePath, int groupNumber, int useAMP, int use422);
extern int readPowerBoard_Ver_for_WatchDog();
extern void setPowerBoardWatchdog(int mode, int OnOff);

#if 0
extern void Timer_Test(timer_t* timerid, void (*Func)(),long msecInterval);
#endif
#ifdef DEF_IP_FILE_DIVIDE_USE
extern void sendBroadcastMsgIPFilePlay(int messageType, char* filePath, int groupNumber, int useAMP, int use422);
#endif

#ifndef ADMIN_CHIME_TIME_DEF
extern int WavReader (char* fileName);
#endif
#ifdef USE_SUJAWON_DEF
char g_suja_ser_ip[30];
extern int ftp_clientmain (char *loc_ip,char *loc_port, char *loc_usernm, char *loc_passwd, char *loc_s_dir, char *loc_filenm, char *loc_dir);
void Sujawon_Recieve_Command_process(int client_sockfd, char *buf);
int Sujawon_server_main();
void suja_req_file_broadcast_msgq(char *file_buf);
void suja_ack_reponse_to_server_by_socket(int client_sockfd, char *ack_buf,int len);
void suja_send_to_server_ip_result_data_proc(char *data,int len);

#endif

#ifdef DEF_FOR_TEST_TIMER
timer_t firstTimerID;
timer_t secondTimerID;
timer_t thirdTimerID;

void for_test_timer_print();
void for_test_timer_print_once();
#endif
#ifdef DEF_USE_LOG_DELETE
void disk_check_run();
int record_file_count_check();
void record_file_remove();
#endif

struct server_braod_mx_data *g_srv_broad_mx;
int send_udp_log_report_data(char *buf);


#ifndef DEF_MODEM_RESTART_NEW_PROC
void sigint_handler( int signo)
{
	kprintf( "Modem CFUN SigAlarm Alert!!\n");
	if(CFUN_flag == 1){
	 	if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 0)
	 		rebootSystemCDMA(1);
		CFUN_flag = 2;
	}
/*
    kprintf( "Modem CFUN SigAlarm Alert!! g_mdm_rst_count= %d\n",g_mdm_rst_count);
	if(CFUN_flag == 1){
	 	if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 0){
#ifdef DEF_MODEM_RESTART_NEW_PROC
			if(g_mdm_rst_count >= 3){
				CFUN_flag = 5;				
			}
			else{
				g_mdm_rst_count++;
				rst_count_add();
				rebootSystemCDMA();
				CFUN_flag = 2;
			}
#else				
	 		rebootSystemCDMA();
			CFUN_flag = 2;
#endif
	 	}
		else
		{
#ifdef DEF_MODEM_RESTART_NEW_PROC		
			CFUN_flag = 5;
#else
			CFUN_flag = 2;
#endif
		}
	}
*/	
}
#endif

/*****************************************************************************
* @brief    main function
* @section  [desc]
*****************************************************************************/
void app_thr_wait_msecs(unsigned int msecs)
{
#if 0
	struct timespec delayTime, elaspedTime;

	delayTime.tv_sec  = msecs/1000;
	delayTime.tv_nsec = (msecs%1000)*1000000;

	int ret = nanosleep(&delayTime, &elaspedTime);
	//int ret = nanosleep(&delayTime, NULL);
	if(ret == -1) kprintf("[%s] nanosleep time error\n",__FUNCTION__);
#else
	struct timeval tv;

	 tv.tv_sec =  msecs/1000;
	 tv.tv_usec = (msecs%1000)*1000;

	 select(0, NULL, NULL, NULL, &tv);
#endif	
}

void app_thr_wait_microsecs(unsigned int microsecs)
{
	struct timeval tv;

	 tv.tv_sec =  microsecs/1000000;
	 tv.tv_usec = microsecs % 1000000;

	 select(0, NULL, NULL, NULL, &tv);
}

int app_thr_wait_event(app_thr_obj *tObj)
{
	THR_semHndl *pSem;
	int r = -1;

	if (!tObj->active)
		return -1;

	pSem = &tObj->sem;

  	pthread_mutex_lock(&pSem->lock);
	while (1) {
		if (pSem->count > 0) {
			pSem->count--;
			r = 0;
			break;
		} else {
			pthread_cond_wait(&pSem->cond, &pSem->lock);
		}
	}
  	pthread_mutex_unlock(&pSem->lock);

	return tObj->cmd;
}

int app_thr_send_event(app_thr_obj *tObj, int cmd, int prm0, int prm1)
{
	THR_semHndl *pSem;
	int r = 0;

	if (tObj == NULL || !tObj->active) {
		return -1;
	}
	tObj->cmd = cmd;
	tObj->param0 = prm0;
	tObj->param1 = prm1;

	pSem = &tObj->sem;
  	pthread_mutex_lock(&pSem->lock);
  	if (pSem->count < pSem->maxCount) {
    	pSem->count++;
		kprintf("[app_thr_send_event]...\n");
    	r |= pthread_cond_signal(&pSem->cond);
  	}
  	pthread_mutex_unlock(&pSem->lock);

	return 0;
}

static int app_main_thr_init(void)
{
	THR_semHndl *pSem;

	pthread_mutexattr_t muattr;
  	pthread_condattr_t cond_attr;

  	int res = 0;

    memset((void *)app_cfg, 0, sizeof(app_cfg_t));

	app_cfg->mObj = (app_thr_obj *)malloc(sizeof(app_thr_obj));
	if (app_cfg->mObj == NULL) {
		eprintf("Failed to alloc main thread obj!!\n");
		return -1;
	}

	pSem = &app_cfg->mObj->sem;

  	res |= pthread_mutexattr_init(&muattr);
  	res |= pthread_condattr_init(&cond_attr);

  	res |= pthread_mutex_init(&pSem->lock, &muattr);
  	res |= pthread_cond_init(&pSem->cond, &cond_attr);

  	pSem->count = 0;
  	pSem->maxCount = MAX_PENDING_SEM_CNT;

  	if (res != 0)
    	eprintf("Failed to pthread mutext init = %d\n", res);

  	pthread_condattr_destroy(&cond_attr);
  	pthread_mutexattr_destroy(&muattr);

	return 0;
}

static void app_main_thr_exit(void)
{
	THR_semHndl *pSem;

	if (app_cfg->mObj != NULL) {
		pSem = &app_cfg->mObj->sem;
		pthread_cond_destroy(&pSem->cond);
  		pthread_mutex_destroy(&pSem->lock);

		free(app_cfg->mObj);
	}
}


static int app_main_cfg_init(void)
{
	int r;

	//# set user config
	app_cfg->snd_ch  = 1;
	app_cfg->snd_rate = 44100;
/*
	r = pipe(app_cfg->snd_pipes);
	if (r < 0) {
		eprintf("can't not init pipes (%d)\n", errno);
	}
*/
	return 0;
}


/*****************************************************************************
* @brief    app menu function
* @section  [desc] console menu
*****************************************************************************/
char main_menu[] = {
	"\r\n ============="
	"\r\n Run-Time Menu"
	"\r\n ============="
	"\r\n"
	"\r\n 0: stop"
	"\r\n"
	"\r\n Enter Choice: "
};

static int app_menu(void)
{
	app_thr_obj *tObj = (app_thr_obj *)app_cfg->mObj;
	int loc_exit = 0, cmd;

	kprintf(" [task] %s start\n", __func__);

	tObj->active = 1;

	while (!loc_exit)
	{
		//# wait cmd
		cmd = app_thr_wait_event(tObj);
		
		kprintf("app_menu()  cmd= %d\n",cmd);
	}

	tObj->active = 0;

	kprintf(" [task] %s stop\n", __func__);

	return SOK;
}

/*----------------------------------------------------------------------------
 application config function
-----------------------------------------------------------------------------*/
int app_cfg_init(void)
{
	memset((void *)iapp, 0, sizeof(app_cfg_t));

	//# set user config

	return SOK;
}

int app_cfg_exit(void)
{
	return SOK;
}

long get_filesize(char *name)
{
   long size;
   int flag;
   struct stat buf;
   flag = stat (name,&buf);
   if (flag == -1) return -1;
   size = buf.st_size;
   return (size);
}

void sigterm_handler(int signo)
{
    kprintf("SIGTERM Generation signo= %d\n",signo);

	signal(SIGINT,SIG_IGN);
  	kill(0,SIGINT);

	exit(EXIT_FAILURE);
}

void Signal_list(void)
{
  signal(SIGQUIT,sigterm_handler);
  signal(SIGINT,sigterm_handler);
  signal(SIGHUP,sigterm_handler);
  signal(SIGSEGV,sigterm_handler);
  signal(SIGBUS,sigterm_handler);
  signal(SIGKILL,sigterm_handler);
  signal(SIGABRT, sigterm_handler);
}


/*****************************************************************************
* @brief    main function
* @section  [desc]
*****************************************************************************/
int main(int argc, char **argv)
{
	int r;
	app_thr_obj *tObj = &iapp->mObj;

	printf("====================================================================\n");
	printf("  Version : MX400B-01A\n");
	printf("====================================================================\n");

	//file_read_date_time();

	Signal_list();
	
#ifdef USE_FILE_LOG
#ifdef DEF_MQUEUE_LOG_USE
	log_data_file_setting();
	if(thread_create(tObj, &mqueue_log_receive_thread, APP_THREAD_PRI, NULL) < 0) {
		eprintf("mqueue_log_receive_thread create thread fail!\n");
	}
#else
	pthread_mutex_init(&g_fi_mutex, NULL ); //swpark 20180110 for log file
	log_data_file_setting();
#endif	
#endif

	libimg_libray_size_check();

#ifdef DEF_RESERVED_BROADCAST_USE
	reserved_file_directory_check_n_creat();
#endif
	kprintf("[%s] Application S/W VER = %s\n\n",__FUNCTION__,SW_APP_VER_STR);
	
	gRegisterFilename = "/usr/broadcast/broadcast";
	g_srv_broad_mx = (struct server_braod_mx_data *)malloc(sizeof(struct server_braod_mx_data));	
	app_cfg = (app_cfg_t *)malloc(sizeof(app_cfg_t));
	if (app_cfg == NULL) {
		fprintf(stderr, "Insufficient memory!!\n");
#ifdef USE_FILE_LOG
		g_log_print_flag = 0;
		app_thr_wait_msecs(100);
#endif					
		system("sync");
		system("reboot");
	}
	sigignore(SIGCHLD);
	sigignore(SIGTERM);
#ifndef DEF_MODEM_RESTART_NEW_PROC
	signal(SIGALRM, sigint_handler);
#endif

	InitAuth();
	app_cfg_init();

	system_gpio_init();
	system_i2c_init();
	
	//#--- system init
	r = app_main_thr_init();
	r |= app_main_cfg_init();
	
	if (r < 0){
		kprintf("app_main_thr_init() & app_main_cfg_init() error!!\n");
#ifdef USE_FILE_LOG
		g_log_print_flag = 0;
		app_thr_wait_msecs(100);
#endif					
		system("sync");
		system("reboot");
	}	
	
	mcfw_linux_init(0);  //시스템 부팅 시 한번만 수행 oae_naw.out restart 시 오류 발생 

	//# app module init
	boardInit();	
	addressinit();
	
#ifdef DEF_MODEM_RESTART_NEW_PROC
	g_mdm_rst_count = checking_modem_restart_count();
	kprintf("====>>> g_mdm_rst_count = %d\n",g_mdm_rst_count);
#endif

#ifndef USE_JIG_TEST	
	system("/root/delfile_naw.sh &"); //swpark boot setting program delite shell
	app_thr_wait_msecs(200);
#endif	
			
	if(access("/mmc/download", 0) == -1){
		system("mkdir /mmc/download/");
	}

	if(thread_create(tObj, (void *) &RS232function, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}
	sleep(1);

	if(g_vpn_config_data.vpn_use_flag == CK_USE){
		if(g_vpn_config_data.vpn_server_ip[0] != 0){
			int ret_val = vpn_config_file_checking_n_write();
		}
	}
	if(g_log_data_send_flag == 1){
		if(access("/mmc/log_report", 0) == -1){
			system("mkdir /mmc/log_report/");
		}
	}
#ifdef DEF_HW_WATCHDOG_USE
#ifdef DEF_OLD_BRD_USE
	g_pwr_hw_ver = readPowerBoard_Ver_for_WatchDog();
	kprintf("[%s] HW Ver = %d\n",__FUNCTION__,g_pwr_hw_ver);
#endif	
#endif

	Modem_Initial_Processing();
	
#ifdef DEF_MODEM_RESTART_NEW_PROC
#ifndef DEF_MODEM_NOT_CONFIG
	if((CFUN_flag == 0) && (g_mdm_rst_count != 0)){
		g_mdm_rst_count = 0;
		rst_count_add();
	}
	//else if((CFUN_flag == 5) || (CFUN_flag == 2)){
	else if(CFUN_flag != 0){
		kprintf("start StartTimer_sys()\n");
		start_modem_reset_n_check(300000);
	}
#endif	
#endif

	Chime_Time_setting();

	sleep(1);
	startWorkThread(CK_ON);
	sleep(1);

	Modem_ip_link_setup();
	
	if(thread_create(tObj, (void *) &BroadcastTfunction, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}
	if(thread_create(tObj, (void *) &ButtonTfunction, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}
	if(thread_create(tObj, (void *) &GuiTfunction, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}
	startThreadBootInit(CK_ON);
#ifndef DEF_HW_WATCHDOG_USE		
#ifndef DEF_WATCHDOG_SYS_TIME_USE
	if(thread_create(tObj, (void *) &watchdog_report_thraead, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}

#endif
#endif
	if(thread_create(tObj, (void *) &time_delay_function, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}

#ifdef DEF_MQUEUE_USE
	if(thread_create(tObj, (void *) &mqueue_aplay_thread, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}
#endif

	app_gui_init();

	StatusProcess_init(); //swpark 2017_0112

	kprintf("gRegisters.Equipment_IP_Address.Equipment_Server_IP.Equipment_Server_IP addr :%s\n",get_server_ip());

	kprintf("gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M = %d\n",gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M);
	kprintf("gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet = %d\n",gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet);

	IP_Broadcast_Thread_Generation(tObj);

	if(get_g_lcd_version_flag() == 0){
		kprintf("lcd configuration resetting!!\n");
		new_lcd_gpio_set();
	}

	if(water_level_data.used_flag == CK_USE){
		if(thread_create(tObj, (void *) &water_level_check_proc, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
		}
	}
#ifdef DEF_DOOR_OPEN_SEND_MSG_USE   //door open SMS report processing
	if(door_check_data.used_flag == CK_USE)
	{
		if(thread_create(tObj, (void *) &door_open_check_proc, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
		}
	}
#endif

#ifndef DEF_OLD_BRD_USE
	if(thread_create(tObj, (void *) &reserved_broadcast_processing, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}
#else  //#ifndef DEF_OLD_BRD_USE
#if (defined(DEF_ORG_UI_USE) || defined(DEF_IN_MODEL_USE))
#ifdef DEF_RESERVED_BROADCAST_USE
	if(g_lib_index_id == 1){
		if(thread_create(tObj, (void *) &reserved_broadcast_processing, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
		}
	}
#endif
#endif
#endif  //#ifndef DEF_OLD_BRD_USE

	write_app_sw_ver_to_broadcast();
	data_buffer_save_n_send_bt80(2, 0, 0);
#ifdef DEF_NET_TIME_USE
	if((CFUN_flag == 5) && (gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1)){
		/*
		char loc_ip_addr[100];
		memset(loc_ip_addr,0,sizeof(loc_ip_addr));
		int loc_ip_ret = get_system_MyIPAddress(loc_ip_addr);
		if(loc_ip_ret == 1){
			kprintf("My system IP Address= %s\n",loc_ip_addr);
			net_time_setting();
		}
		else kprintf("My system IP Address is not setting\n");
		*/
		//if(loc_ip_ret_eth0 == 1) net_time_setting();
	}
#endif

#ifdef DEF_AUTO_REBOOT_TEST
/*
	if(thread_create(tObj, (void *) &Auto_reboot_proc, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
	} */
	Auto_reboot_proc();
#endif

#if 0
	char l_char = 's';
  	unsigned short l_mode = oae_crypt("20170524","12345678",l_char);
  	kprintf("mode = %d\n",l_mode);
	l_char = 't';
	l_mode = oae_crypt("20170524","12345678",l_char);
  	kprintf("mode = %d\n",l_mode);
#endif
#if 0
	timer_t test_timer;
	Timer_Test(&test_timer, 3000);
#endif

	//#--- app main ----------
	app_menu(); //swpark
	//#-----------------------

	startWorkThread(CK_OFF);
		
	//# app module exit
	app_gui_exit();

	mcfw_linux_exit();
	app_key_exit();

	app_main_thr_exit();
//	app_main_gpio_exit();
	system_gpio_exit();

	free(app_cfg);
#ifdef USE_FILE_LOG
#ifndef DEF_MQUEUE_LOG_USE
	pthread_mutex_destroy(&g_fi_mutex );
#endif
#endif
	kprintf("--- NAW end ---\n\n");

	return 0;
}

void libimg_libray_size_check()
{
#ifdef DEF_OLD_BRD_USE
	g_libimg_db_size = get_filesize("/usr/local/lib/libimg_db.so");
	kprintf("/usr/local/lib/libimg_db.so size = %ld, Product Company Model= %s\n",g_libimg_db_size,DEF_PROCDUCT_COMP);
#ifdef DEF_ORG_UI_USE	
	if((g_libimg_db_size > 11000000) && (g_libimg_db_size < 13000000)){
		g_lib_index_id = 1;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
	else if((g_libimg_db_size < 9750000) || (g_libimg_db_size > 13000000)){
		g_lib_index_id = -1;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
#elif defined(DEF_IN_MODEL_USE)		
	if(g_libimg_db_size > 14000000){
		g_lib_index_id = 1;
		kprintf("[%s] [DEF_IN_MODEL_USE] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
	else if(g_libimg_db_size < 13000000){
		g_lib_index_id = -1;
		kprintf("[%s] [DEF_IN_MODEL_USE] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
#elif defined(DEF_H_MODEL_USE)	
	if((g_libimg_db_size > 13200000) && (g_libimg_db_size < 13400000)){
		g_lib_index_id = 0;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
	else {
		g_lib_index_id = -1;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
#elif defined(DEF_Y_MODEL_USE)	
	if((g_libimg_db_size > 12900000) && (g_libimg_db_size < 13100000)){
		g_lib_index_id = 0;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
	else {
		g_lib_index_id = -1;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}	
#elif defined(DEF_U_MODEL_USE)	
	if((g_libimg_db_size > 12400000) && (g_libimg_db_size < 12500000)){
		g_lib_index_id = 0;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
	else {
		g_lib_index_id = -1;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}		
#elif defined(DEF_M_MODEL_USE)	
	if((g_libimg_db_size > 11700000) && (g_libimg_db_size < 11800000)){
		g_lib_index_id = 0;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}
	else {
		g_lib_index_id = -1;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}			
#endif		//#ifdef DEF_ORG_UI_USE

#else
	g_libimg_db_size = get_filesize("/usr/local/lib/libimg_db.so");
	kprintf("/usr/local/lib/libimg_db.so size = %ld, Product Company Model= %s\n",g_libimg_db_size,DEF_PROCDUCT_COMP);
	if(g_libimg_db_size < 9640000){
		g_lib_index_id = -1;
		kprintf("[%s] g_lib_index_id= %d\n",__FUNCTION__,g_lib_index_id);
	}			
#endif  //#ifdef DEF_OLD_BRD_USE
}

#ifdef DEF_HW_WATCHDOG_USE
#ifdef DEF_OLD_BRD_USE
void HW_Watchdog_processing()  //MX400 HW watchdog 처리 
{
	kprintf("[%s] H/W Watchdog Start!!\n",__FUNCTION__);
	
	setPowerBoardOnOff(SET_POWER_BOARD_ON_OFF_WDT_EN, CK_OFF);	//watcgdog enable 
	app_thr_wait_msecs(100);
	repeat_Wdt_wdi_On_OFF();
}

#else
void HW_Watchdog_processing()
{
	int val =0;
	kprintf("[%s] H/W Watchdog Start!!\n",__FUNCTION__);
	
	val = I2C_Read_Data_from_Device_Register(3);
	if(val != -1){
		val = val & 0xFFFC;
		val = val | 0xFF48;
		I2C_Write_Data_from_Device_Register(3,val,2);
	}
	app_thr_wait_msecs(500);
	while(1){
		val = 0;
		
#ifdef DEF_FORK_PROCESS_USE
		fd_islock(i2cHndl_PWD.fd);
#endif					
		val = I2C_Read_Data_from_Device_Register(2);
#ifdef DEF_FORK_PROCESS_USE
		fd_unlock(i2cHndl_PWD.fd);
#endif
		if(val != -1){		
			val = val | 0xFF49;
			//app_thr_wait_msecs(5);
#ifdef DEF_FORK_PROCESS_USE
			fd_islock(i2cHndl_PWD.fd);
#endif		
			I2C_Write_Data_from_Device_Register(2,val,2);
#ifdef DEF_FORK_PROCESS_USE
			fd_unlock(i2cHndl_PWD.fd);
#endif
			app_thr_wait_msecs(600);
			
#ifdef DEF_FORK_PROCESS_USE
			fd_islock(i2cHndl_PWD.fd);
#endif	
			int val_1 = I2C_Read_Data_from_Device_Register(2);
#ifdef DEF_FORK_PROCESS_USE
			fd_unlock(i2cHndl_PWD.fd);
#endif
			//app_thr_wait_msecs(5);
			if(val_1 != -1){
				val_1 = val_1 & 0xFFFE;
				val_1 = val_1 | 0xFF48;
				
#ifdef DEF_FORK_PROCESS_USE
				fd_islock(i2cHndl_PWD.fd);
#endif
				I2C_Write_Data_from_Device_Register(2,val_1,2);
#ifdef DEF_FORK_PROCESS_USE
				fd_unlock(i2cHndl_PWD.fd);
#endif
			}
			else{
				kprintf("[%s] H/W Watchdog Read WDT_WDI(0) Error!!\n",__FUNCTION__);
			}
		}
		else{
			kprintf("[%s] H/W Watchdog Read WDT_WDI(1) Error!!\n",__FUNCTION__);
		}
		app_thr_wait_msecs(600);
		//sleep(1);
	}
}
#endif
#endif


#ifdef DEF_AUTO_REBOOT_TEST
int Auto_reboot_proc()
{
	sleep(120);
	printf("==================>>>>> Auto_reboot_proc\n");
	rebootSystemCDMA(0);
	//system("sync");
	//system("reboot");
}
#endif

#ifdef USE_FILE_LOG
void log_data_file_setting()
{
	if(access("/usr/local/log", 0) == -1){
		system("mkdir /usr/local/log");
	}
	struct timeval loc_time_t;
	struct tm loc_time_tm;
	gettimeofday(&loc_time_t, NULL);
#ifdef DEF_UTC_TIME_ZONE	
	gmtime_r(&loc_time_t.tv_sec, &loc_time_tm);
#else
	localtime_r(&loc_time_t.tv_sec, &loc_time_tm);
#endif
	char log_date[200];
	sprintf(log_date,"/usr/local/log/%4d%02d%02d.log",loc_time_tm.tm_year+1900,loc_time_tm.tm_mon+1,loc_time_tm.tm_mday);
	kprintf("log_date = %s\n",log_date);
	if(access(log_date, F_OK) < 0){
		int loc_file_fd;
		if ( 0 < ( loc_file_fd = creat( log_date, 0644))) close(loc_file_fd);
	}
	 
	SetKLogFile(log_date);
}
#endif	

void time_delay_function()
{
	sleep(2);
	startThreadBootInit(CK_OFF);
#ifndef DEF_HW_WATCHDOG_USE	
#ifdef DEF_WATCHDOG_SYS_TIME_USE	
#ifdef DEF_WATCHDOG_TIME_TEST
	StartTimer_sys(&g_wtd_s_timer,&setWDT_KeepAlive,3000);		// 3초마다 watchdog timer setting
#else
#ifndef DEF_SYSTEM_TIMER_THREAD_USE
	StartTimer_sys(&g_wtd_s_timer,&setWDT_KeepAlive,WATCHDOG_TIME_INTERVAL*1000);		// 113초마다 watchdog timer setting
#else
	StartTimer_sys(&g_wtd_s_timer,&setWDT_KeepAlive_handle,WATCHDOG_TIME_INTERVAL*1000);		// 113초마다 watchdog timer setting
#endif
	//StartTimer_sys(&g_wtd_s_timer,&setWDT_KeepAlive,10*1000);		// 113초마다 watchdog timer setting
#endif	
#endif		
#endif  //#ifndef DEF_HW_WATCHDOG_USE
	
#ifdef DEF_USE_LOG_DELETE
#if 1
	kprintf("disk_check.sh run start!!!!\n");
#ifndef DEF_MODEM_TEST_LOG_DELETE
#if 1
	int ret = record_file_count_check();
	if(ret > 200){
		record_file_remove();
	}
#endif	
#else
#ifndef DEF_WATCHDOG_TIME_TEST
	system("/bin/rm /mmc/record/*");
	system("sync");
	system("sync");
#endif	
#endif
	if(access("/usr/local/bin/disk_check.sh", F_OK) < 0) {
		system("/usr/bin/find /mmc/record -mtime +60 -exec /bin/rm {} \;");
		system("/usr/bin/find /mmc/record -size +2000000000c -exec /bin/rm {} \;");
	}
	else{
		if(access("/usr/local/bin/disk_check.sh", X_OK) < 0){
			system("chmod 777 /usr/local/bin/disk_check.sh");
		}
		system("/usr/local/bin/disk_check.sh");
	}
	kprintf("disk_check.sh run end\n");
#else
	int disk_pid = fork();
	kprintf("disk_check.sh run!!, disk_pid =%d\n",disk_pid);
	if(disk_pid == 0) {
		disk_check_run();
	}
#endif	
#endif	

#ifdef DEF_HW_WATCHDOG_USE
#ifdef DEF_FORK_PROCESS_USE  //watchdog 실행을 프로세스로 동작하게 함 
#ifdef DEF_OLD_BRD_USE	
	
	if(g_pwr_hw_ver == 1){
		int hwwatch_pid = fork();

		if(hwwatch_pid == 0) {
			HW_Watchdog_processing();
		}
		else if(hwwatch_pid > 0){
			g_hwwatch_pid = hwwatch_pid;
		}
	}
	else{
		setEnableWDT(130);
		
		if(thread_create(_tobjHWwatchdogThread, (void *) &watchdog_report_thraead, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
		}
	}
#else
	int hwwatch_pid = fork();
	if(hwwatch_pid == 0) {
		HW_Watchdog_processing();
	}
	else if(hwwatch_pid > 0){
		g_hwwatch_pid = hwwatch_pid;
	}
#endif
#else
	if(thread_create(_tobjHWwatchdogThread, (void *) &HW_Watchdog_processing, APP_THREAD_PRI, NULL) < 0) {
		eprintf("create thread\n");
	}
#endif
	
#endif

}	

#ifdef DEF_SYSTEM_TIMER_THREAD_USE
void setWDT_KeepAlive_handle(union sigval si)
{
	setWDT_KeepAlive();
}

void for_modem_init_timer_proc_handle(union sigval si)
{
	for_modem_init_timer_proc();
}
#endif

#ifdef DEF_USE_LOG_DELETE
void disk_check_run()
{
	kprintf("disk_check.sh run start!!!!\n");
	int ret = record_file_count_check();
	if(ret > 200){
		record_file_remove();
	}
	if(access("/usr/local/bin/disk_check.sh", F_OK) < 0) {
		system("/usr/bin/find /mmc/record -mtime +60 -exec /bin/rm {} \;");
		system("/usr/bin/find /mmc/record -size +2000000000c -exec /bin/rm {} \;");
	}
	else{
		if(access("/usr/local/bin/disk_check.sh", X_OK) < 0){
			system("chmod 777 /usr/local/bin/disk_check.sh");
		}
		system("/usr/local/bin/disk_check.sh");
	}
	kprintf("disk_check.sh run end\n");
}

int record_file_count_check()
{
	int ret_val = 0;
	FILE *fp;
	char buffer[100];
	
	fp = popen("ls /mmc/record | wc -l", "r");
	if(NULL == fp){
		kprintf("[%s] %s\n", __FUNCTION__, "popen() fail");
		return -1;
	}
	memset(buffer,0,sizeof(buffer));
	while(fgets( buffer, sizeof(buffer), fp)){
		kprintf("[%s] /mmc/record file count = %s\n", __FUNCTION__, buffer);
		ret_val = atoi(buffer);
		break;
	}

	pclose(fp); 

	return ret_val;
}

void record_file_remove()
{
	char buff[300];
	if(access("/mmc/record_list", F_OK) >= 0) {
		system("/bin/rm /mmc/record_list");
	}
	system("ls -t /mmc/record/*.raw | tail -n +200 | grep raw >> /mmc/record_list");
	system("sync");
	FILE *fp = fopen("/mmc/record_list", "rb");
	memset(buff,0,sizeof(buff));
	while(fgets( buff, sizeof(buff), fp)){
		//printf("[%s] %s\n", __FUNCTION__, buff);
		char loc_buf[300];
		memset(loc_buf,0,sizeof(loc_buf));
		sprintf(loc_buf,"/bin/rm %s", buff);
		system(loc_buf);		
		memset(buff,0,sizeof(buff));
	}
	fclose(fp);
	system("sync");
	system("sync");
	kprintf("==> record file remove OK\n");
}

#endif

void addressinit(void)
{
	startaddr[0] = 0x3000;
	startaddr[1] = 0x3100;
	startaddr[2] = 0x3200;
	startaddr[3] = 0x3300;
	startaddr[4] = 0x3400;
	startaddr[5] = 0x1000;
	startaddr[6] = 0x2100;
	startaddr[7] = 0x2200;
	startaddr[8] = 0x2300;
	startaddr[9] = 0x2400;
	startaddr[10] = 0x2500;
	startaddr[11] = 0x5000;
	startaddr[12] = 0x5200;
	startaddr[13] = 0x5300;
	startaddr[14] = 0x6000;
	startaddr[15] = 0x7000;
	
	startaddr[16] = 0xC000;
	startaddr[17] = 0x2600;
	startaddr[18] = 0x2800;

	
	stopaddr[0] = 0x30FF;
	stopaddr[1] = 0x3180;
	stopaddr[2] = 0x320F;
	stopaddr[3] = 0x333F;
	stopaddr[4] = 0x347F;
	stopaddr[5] = 0x1FFF;
	stopaddr[6] = 0x213F;
	stopaddr[7] = 0x222F;
	stopaddr[8] = 0x233F;
	stopaddr[9] = 0x241F;
	stopaddr[10] = 0x250F;
	stopaddr[11] = 0x50FF;
	stopaddr[12] = 0x522F;
	stopaddr[13] = 0x532F;
	stopaddr[14] = 0x60FF;
	stopaddr[15] = 0x7FFF;
	
	stopaddr[16] = 0xCFFF;
	stopaddr[17] = 0x26FF;
	stopaddr[18] = 0x28FF;
}

#ifndef USE_SUJAWON_DEF
int socket_client_main()
{
	struct sockaddr_in serveraddr;
	int server_sockfd;
	int client_len;
	char buf[SOC_REGLEN];
	kprintf("socket_client_main() waken!!!\n");
	int init_client_reg_ip = 0,init_report=0;

	while(1){
#ifndef DEF_IP_REPORT_TEST		
		if(init_client_reg_ip == 1){
			sleep(g_ip_report_time);  //20180716 30분에서 10분으로 단축 			
		}
		else{
			init_client_reg_ip = 1;
			sleep(5);			
			if(strcmp(gMyCallNumber,"") == 0)
				read_IP_ID_value_to_gMyCallNumber();
		}
#else
		sleep(2);
#endif
		if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet ==1)
		{
			if(strcmp(get_server_ip(),"0.0.0.0") == 0) 
			{
				kprintf("[%s]server IP not setting yet\n",__FUNCTION__);
				continue;
			}
			if(strcmp(gMyCallNumber,"") == 0)
			{
				init_client_reg_ip = 0;
				kprintf("My Call Number is not setting yet!!!\n");
				read_IP_ID_value_to_gMyCallNumber_no_check();
				continue;
			}
			kprintf("[Reg_IP] start : server ip= %s\n",get_server_ip());
			if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)	
			{
				kprintf("socket error : %s\n",strerror(errno));
				init_client_reg_ip = 0;
				continue;
			}
			serveraddr.sin_family = AF_INET;
			serveraddr.sin_addr.s_addr = inet_addr(get_server_ip());
			serveraddr.sin_port = htons(13500);
			
			client_len = sizeof(serveraddr);
			
			if(connect(server_sockfd, (struct sockaddr *)&serveraddr, client_len) == -1)
			{
				kprintf("connect error: %s\n",strerror(errno));
				close(server_sockfd);
				init_client_reg_ip = 0;
				if(getCurrentBroadcastState() >= BOARDCAST_STATE_TTS_PLAY_START){
					g_rcv_flag_from_ip_server = 1;
					stop_currently_runing_service(-1,0);
				}
				continue;
			}
			
			memset(buf, 0x00, SOC_REGLEN);	
			struct ip_req_data *loc_req = (struct ip_req_data *) buf;
			sprintf(loc_req->cmd_id,"REG_IP");
			loc_req->from_id = 0x02;
			loc_req->port_id = get_g_server_ipcomm_port();
			
			strcpy(loc_req->tel_num,gMyCallNumber);
			int i=0;
			
			kprintf("!! %s\n",gMyCallNumber);

			if(write(server_sockfd, buf, SOC_REGLEN) <= 0)
			{
				kprintf("write error: %s\n",strerror(errno));
				close(server_sockfd);
				continue;
			}			
			memset(buf, 0x00, SOC_REGLEN);
			int r_len=0;
#ifdef DEF_NEW_IP_REG_METHOD	
			struct    timeval tv; 
		    fd_set    readfds;
			int loc_flag = 1;
		    
		    FD_ZERO(&readfds);

		    FD_SET(server_sockfd, &readfds);

	        tv.tv_sec = 5;
	        tv.tv_usec = 0;

	        int loc_state = select(server_sockfd + 1, &readfds, (fd_set *)0, (fd_set *)0, &tv);
	        switch(loc_state)
	        {
	            case -1:
	                kprintf("[%s] select error : \n",__FUNCTION__);
					close(server_sockfd);
					loc_flag = 1;
					init_client_reg_ip = 0;
	                break;    
	            case 0:
	                kprintf("[%s] Time over\n",__FUNCTION__);            
	                close(server_sockfd);	
					loc_flag = 1;
					init_client_reg_ip = 0;
	                break;
	            default:
	                if((r_len = read(server_sockfd, buf, SOC_REGLEN)) <= 0)
					{
						kprintf("[%s] read error : %s\n",__FUNCTION__,strerror(errno));
						close(server_sockfd);
						init_client_reg_ip = 0;
						loc_flag = 1;
					}
					else loc_flag = 0;
	                break;
	        }
			if(loc_flag == 1) continue;
#else
			if((r_len = read(server_sockfd, buf, SOC_REGLEN)) <= 0)
			{
				kprintf("read error : %s\n",strerror(errno));
				close(server_sockfd);
				continue;
			}
#endif			
			close(server_sockfd);	
			kprintf("ip_reg ack : %s\n",buf);
#ifndef DEF_IP_REPORT_TEST						
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"STMN1,%s,NULL,IP REGISTRATION REQUEST",gMyCallNumber);
			udp_message_data_send_proc(temp_log,1);
			if(init_report == 0){
				ip_status_report_proc(3);
				init_report = 1;
			}
#endif	
		}
	}
	return 0;
}

int socket_server_main()
{	
	int server_sockfd, client_sockfd;
	int client_len, n;
	char buf[SOC_MAXRECVBUF],ack_buf[SOC_MAXBUF];
	struct sockaddr_in clientaddr, serveraddr;
	pid_t pid;
	kprintf("socket_server_main() waken!!!\n");
	
	if((server_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		kprintf("socket error: %s\n",strerror(errno));
		return 1;
	}
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(get_g_server_ipcomm_port());
	
	int enable = 1;
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		kprintf("setsockopt(SO_REUSEADDR) failed");
	
	bind(server_sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	listen(server_sockfd, 1024);
	client_len = sizeof( clientaddr);
	while(1)
	{
		memset(buf, 0x00, SOC_MAXRECVBUF);
		client_sockfd = accept(server_sockfd, (struct sockaddr *)&clientaddr,(socklen_t*)&client_len);
		kprintf("New Client Connect : %s\n", inet_ntoa(clientaddr.sin_addr));
#if 1		
		int enable = 1;
		if (setsockopt(client_sockfd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(int)) < 0)
			kprintf("setsockopt(SO_KEEPALIVE) failed\n");
		
		int time_e = 100;
		if (setsockopt(client_sockfd, SOL_TCP, TCP_KEEPIDLE, &time_e, sizeof(time_e)) < 0)
			kprintf("setsockopt(SO_KEEPIDLE) failed\n");

		int keep_cnt = 2;
		if (setsockopt(client_sockfd, SOL_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt)) < 0)
			kprintf("setsockopt(SO_KEEPIDLE) failed\n");

		int keep_intv = 30;
		if (setsockopt(client_sockfd, SOL_TCP, TCP_KEEPINTVL, &keep_intv, sizeof(keep_intv)) < 0)
			printf("setsockopt(SO_KEEPIDLE) failed\n");
#endif				
		if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1)
		{
			if((n = read(client_sockfd, buf, SOC_MAXRECVBUF)) <= 0)
			{
				close(client_sockfd);
				kprintf("MX400 sever receive read data error\n");
				continue;
			}
			Recieve_Command_from_IP_Server_process(client_sockfd, buf);
		}
		if(g_live_client_sockfd == -1){
			close(client_sockfd);	
		}	
	}
	close(server_sockfd);	
	return 0;
}


void Recieve_Command_from_IP_Server_process(int client_sockfd, char *buf)
{
	char ack_buf[SOC_MAXBUF],ack_play_buf[24];
	struct ip_cmd_data *loc_cmd;
	loc_cmd = (struct ip_cmd_data *)buf;
	int i=0;
	kprintf("server receive data : %s\n",loc_cmd->cmd_id);
	
	if(strcmp(loc_cmd->cmd_id,"STOP_CMD") == 0)
	{
		struct stop_req_mx_data *loc_stop;
		loc_stop = (struct stop_req_mx_data *)buf;
		set_g_SunCha_Mode(CK_OFF);
		sprintf(g_str_guid,"%s",loc_stop->guid);
		g_rcv_flag_from_ip_server = 1;
		stop_currently_runing_service(client_sockfd,loc_stop->idx);	
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"STOP_ACK");
		ack_reponse_to_server_by_socket(client_sockfd,ack_buf);
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL1,%s,%s,BROADCASTING STOP INDICATION",gMyCallNumber,g_str_guid);
		udp_message_data_send_proc(temp_log,5);
		g_play_time = 0;
		
		g_live_client_sockfd = -1;
	}
	else if(strcmp(loc_cmd->cmd_id,"STATUS_REQ") == 0)
	{			
		char sts_buf[SOC_REGLEN];
		
		memset(sts_buf, 0x00, SOC_REGLEN);	
		struct ip_status_report *loc_ack = (struct ip_status_report *) sts_buf;
		sprintf(loc_ack->cmd_id,"STATUS_REPORT");
		loc_ack->from_id = 0x02;
		loc_ack->status= (char)g_t_status;		
		strcpy(loc_ack->tel_num,gMyCallNumber);		
		ack_reponse_to_server_by_socket(client_sockfd,sts_buf);		
	}
	else if(strcmp(loc_cmd->cmd_id,"FILE_BROAD_CMD") == 0)
	{
		set_g_SunCha_Mode(CK_OFF);
		
		
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,2);
		}
		struct server_braod_mx_data *ser_ip;
		ser_ip = (struct server_braod_mx_data *)buf;
		kprintf("report_seq = %d\n",ser_ip->report_seq);
		memcpy(g_srv_broad_mx,ser_ip,sizeof(struct server_braod_mx_data));
		sprintf(g_str_guid,"%s",ser_ip->guid);
		g_try_count = ser_ip->try_count;

		memset(ack_play_buf,0,24);
		struct ip_play_ack_data *loc_play_ack;
		loc_play_ack = (struct ip_play_ack_data *)ack_play_buf;
		sprintf(loc_play_ack->cmd_id,"FILE_ACK");
		loc_play_ack->try_count = ser_ip->try_count;
		loc_play_ack->report_seq = ser_ip->report_seq;
		ack_reponse_to_server_by_socket_24(client_sockfd,ack_play_buf);
		
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL4,%s,%s,INTERNAL FILE BROADCASTING REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);
		udp_message_data_send_proc(temp_log,5);
		
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_FILE_PLAY);
		req_file_broadcast_msgq(ser_ip->file_name);
	}
	else if(strcmp(loc_cmd->cmd_id,"TTS_BROAD_CMD") == 0)
	{			
		set_g_SunCha_Mode(CK_OFF);
		
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,2);	
		}
		struct server_braod_mx_data *ser_ip;
		ser_ip = (struct server_braod_mx_data *)buf;
		kprintf("report_seq = %d\n",ser_ip->report_seq);
		memcpy(g_srv_broad_mx,ser_ip,sizeof(struct server_braod_mx_data));
		sprintf(g_str_guid,"%s",ser_ip->guid);
		g_play_time = ser_ip->play_time;		
		g_try_count = ser_ip->try_count;
		printf("g_try_count= %d\n",g_try_count);
		
		memset(ack_play_buf,0,24);
		struct ip_play_ack_data *loc_play_ack;
		loc_play_ack = (struct ip_play_ack_data *)ack_play_buf;
		sprintf(loc_play_ack->cmd_id,"TTS_ACK");
		loc_play_ack->try_count = ser_ip->try_count;
		loc_play_ack->report_seq = ser_ip->report_seq;
		ack_reponse_to_server_by_socket_24(client_sockfd,ack_play_buf);
				
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL5,%s,%s,TTS CHARATERS BROADCASTING REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);
		udp_message_data_send_proc(temp_log,5);
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_TTS_PLAY);
		req_tts_broadcast_msgq(ser_ip->file_name);
	}
	else if(strcmp(loc_cmd->cmd_id,"LIVE_BROAD_CMD") == 0)
	{
		set_g_SunCha_Mode(CK_OFF);
		g_live_client_sockfd = client_sockfd;
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,2);	
		}
		struct server_braod_mx_data *ser_ip;
		ser_ip = (struct server_braod_mx_data *)buf;
		kprintf("report_seq = %d\n",ser_ip->report_seq);
		memcpy(g_srv_broad_mx,ser_ip,sizeof(struct server_braod_mx_data));
		sprintf(g_str_guid,"%s",ser_ip->guid);
		g_try_count = ser_ip->try_count;
		printf("g_try_count= %d\n",g_try_count);
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL6,%s,%s,LIVE VOICE BROADCASTING REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);				
		udp_message_data_send_proc(temp_log,5);
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_LIVE_PLAY);
		req_live_broadcast_msgq(ser_ip->file_name);
	}
	else if(strcmp(loc_cmd->cmd_id,"LIVE_BROAD_ACT") == 0)
	{
		set_g_SunCha_Mode(CK_OFF);
		sprintf(g_str_guid,"%s",loc_cmd->guid);
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"LIVE_ACT_ACK");
		ack_reponse_to_server_by_socket(client_sockfd,ack_buf);
		printf("send to IP server message LIVE_ACT_ACK\n");		
		req_live_broadcast_act_msgq();
	}
	else if(strcmp(loc_cmd->cmd_id,"REC_LIVE_CMD") == 0)
	{			
		set_g_SunCha_Mode(CK_OFF);
		
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,2);
		}

		struct server_rec_live_mx_data *ser_ip;
		ser_ip = (struct server_rec_live_mx_data *)buf;
		kprintf("report_seq = %d\n",ser_ip->report_seq);
		
		sprintf(g_srv_broad_mx->send_date,"%s",ser_ip->send_date);
		sprintf(g_srv_broad_mx->guid,"%s",ser_ip->guid);
		sprintf(g_srv_broad_mx->cmd_id,"%s",ser_ip->cmd_id);
		g_srv_broad_mx->report_seq = ser_ip->report_seq;
		g_play_time = g_srv_broad_mx->play_time = ser_ip->play_time;
		g_try_count = g_srv_broad_mx->try_count = ser_ip->try_count;
		sprintf(g_srv_broad_mx->file_name,"%s",ser_ip->file_name);		
		sprintf(g_str_guid,"%s",ser_ip->guid);		
		g_rec_pre_time = ser_ip->pre_time;
		kprintf("==============> g_rec_pre_time = %d\n",g_rec_pre_time);
		memset(ack_play_buf,0,24);
		struct ip_play_ack_data *loc_play_ack;
		loc_play_ack = (struct ip_play_ack_data *)ack_play_buf;
		sprintf(loc_play_ack->cmd_id,"REC_LIVE_ACK");
		loc_play_ack->try_count = ser_ip->try_count;
		loc_play_ack->report_seq = ser_ip->report_seq;
		ack_reponse_to_server_by_socket_24(client_sockfd,ack_play_buf);
		
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL7,%s,%s,SAVED LIVE AUDIO FILE BROADCASTING REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);				
		udp_message_data_send_proc(temp_log,5);
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_REC_LIVE_PLAY);
		req_rec_live_broadcast_msgq(ser_ip->file_name);
	}
	else if(strcmp(loc_cmd->cmd_id,"RMT_RST_CMD") == 0)
	{			
		sprintf(g_str_guid,"%s",loc_cmd->guid);
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"RMT_RST_ACK");
		ack_reponse_to_server_by_socket(client_sockfd,ack_buf);

		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL8,%s,%s,REMOTE RESET REQUEST",gMyCallNumber,g_str_guid);				
		udp_message_data_send_proc(temp_log,5);		
		
		rebootSystemCDMA(0);
	}
	else if(strcmp(loc_cmd->cmd_id,"AUD_FILE_LIST") == 0)
	{
		sprintf(g_str_guid,"%s",loc_cmd->guid);
		char file_ack_buf[SOC_MAXFILEBUF];
		memset(file_ack_buf,0,SOC_MAXFILEBUF);
		struct file_list_ack_data *loc_ack;
		loc_ack = (struct file_list_ack_data *)file_ack_buf;
		sprintf(loc_ack->cmd_id,"FILE_LIST_ACK");
		Read_IP_File_List(file_ack_buf);
		sprintf(loc_ack->tel_num,"%s",gMyCallNumber);
		kprintf("Read file num = %d\n",loc_ack->num);
		file_list_ack_reponse_to_server_by_socket(client_sockfd,file_ack_buf,(36 + loc_ack->num *32) );
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL9,%s,%s,FILE LIST REQUEST",gMyCallNumber,g_str_guid);
		udp_message_data_send_proc(temp_log,5);					
	}
	else if(strcmp(loc_cmd->cmd_id,"AUD_FILE_DEL") == 0)
	{
		char str_tmp[200];
		struct aud_file_del_data *ser_ip;
		ser_ip = (struct aud_file_del_data *)buf;
		//sprintf(str_tmp,"rm /mmc/IP_ment/%s",ser_ip->file_name);
		//system(str_tmp);
		sprintf(str_tmp,"/mmc/IP_ment/%s",ser_ip->file_name);
		unlink(str_tmp);
		kprintf("IP AUD_FILE_DEL : %s\n",str_tmp);
		sprintf(g_str_guid,"%s",ser_ip->guid);
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"FILE_DEL_ACK");
		ack_reponse_to_server_by_socket(client_sockfd,ack_buf);
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL10,%s,%s,IP AUDIO FILE DELETE REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);
		udp_message_data_send_proc(temp_log,5);					
	}
	else if(strcmp(loc_cmd->cmd_id,"AUD_FILE_ADD") == 0)
	{				
		struct aud_file_add_data *ser_ip;
		ser_ip = (struct aud_file_add_data *)buf;				
		kprintf("IP AUD_FILE_ADD : %s\n",ser_ip->file_name);				
		int ret_val = db_read_add_audio_file_to_local_file(ser_ip->file_name);
		sprintf(g_str_guid,"%s",ser_ip->guid);
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"FILE_ADD_ACK");
		ack_reponse_to_server_by_socket(client_sockfd,ack_buf);
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL11,%s,%s,IP AUDIO FILE ADD REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);
		udp_message_data_send_proc(temp_log,5);					
	}
	else if(strcmp(loc_cmd->cmd_id,"SIREN_PLAY_CMD") == 0)
	{
		set_g_SunCha_Mode(CK_OFF);
		
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,2);
		}

		struct siren_braod_mx_data *ser_ip;
		ser_ip = (struct siren_braod_mx_data *)buf;
		sprintf(g_str_guid,"%s",ser_ip->guid);
		unsigned char siren_id = ser_ip->siren_id;		
		
		sprintf(g_srv_broad_mx->send_date,"%s",ser_ip->send_date);
		sprintf(g_srv_broad_mx->guid,"%s",ser_ip->guid);
		sprintf(g_srv_broad_mx->cmd_id,"%s",ser_ip->cmd_id);
		g_srv_broad_mx->report_seq = ser_ip->report_seq;
		g_play_time = g_srv_broad_mx->play_time = ser_ip->play_time;
		g_try_count = g_srv_broad_mx->try_count = ser_ip->try_count;
		sprintf(g_srv_broad_mx->file_name,"%d",ser_ip->siren_id);
		//kprintf("====================> SIREN_PLAY_CMD report_seq = %d, send_date= %s\n",g_srv_broad_mx->report_seq,g_srv_broad_mx->send_date);

		memset(ack_play_buf,0,24);
		struct ip_play_ack_data *loc_play_ack;
		loc_play_ack = (struct ip_play_ack_data *)ack_play_buf;
		sprintf(loc_play_ack->cmd_id,"SIREN_ACK");
		loc_play_ack->try_count = ser_ip->try_count;
		loc_play_ack->report_seq = ser_ip->report_seq;
		ack_reponse_to_server_by_socket_24(client_sockfd,ack_play_buf);

		char file_name[30];
		if((siren_id == 0) || (siren_id == 1) || (siren_id == 2) || (siren_id == 5)){
			sprintf(file_name,"%d.mp3",siren_id);
		}
		else{
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"CFIL13,%s,%s,SIREN FILE PLAY REQUEST,Unknown Index",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,5);
			return;
		}
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL13,%s,%s,SIREN FILE PLAY REQUEST,%s",gMyCallNumber,g_str_guid,file_name);
		udp_message_data_send_proc(temp_log,5);
		
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_SIREN_PLAY);
		req_siren_broadcast_msgq(file_name);
		
	}
	else if(strcmp(loc_cmd->cmd_id,"UPDATE_REQ") == 0)
	{				
		struct update_request_data *ser_ip;
		ser_ip = (struct update_request_data *)buf;				
		
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_update_result *loc_ack;
		loc_ack = (struct ip_update_result *) ack_buf;
		sprintf(loc_ack->cmd_id,"UPDATE_OK");
		ack_reponse_to_server_by_socket(client_sockfd,ack_buf);				
	}
	else if(strcmp(loc_cmd->cmd_id,"RESERVE_BROAD") == 0)
	{			
		set_g_SunCha_Mode(CK_OFF);
		
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,2);
		}

		struct server_braod_mx_data *ser_ip;
		ser_ip = (struct server_braod_mx_data *)buf;
		kprintf("report_seq = %d\n",ser_ip->report_seq);
		memcpy(g_srv_broad_mx,ser_ip,sizeof(struct server_braod_mx_data));
		sprintf(g_str_guid,"%s",ser_ip->guid);
		g_play_time = ser_ip->play_time;		
		g_try_count = ser_ip->try_count;
		printf("g_try_count= %d\n",g_try_count);
		
		memset(ack_play_buf,0,24);
		struct ip_play_ack_data *loc_play_ack;
		loc_play_ack = (struct ip_play_ack_data *)ack_play_buf;
		sprintf(loc_play_ack->cmd_id,"RESERVE_ACK");
		loc_play_ack->try_count = ser_ip->try_count;
		loc_play_ack->report_seq = ser_ip->report_seq;
		ack_reponse_to_server_by_socket_24(client_sockfd,ack_play_buf);
				
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL12,%s,%s,RESERVED AUDIO FILE BROADCASTING REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);
		udp_message_data_send_proc(temp_log,5);
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_RESERVE_PLAY);
		req_reserve_broadcast_msgq(ser_ip->file_name);
	}
	else if(strcmp(loc_cmd->cmd_id,"EMERGENCY_MIC") == 0)
	{
		set_g_SunCha_Mode(CK_OFF);
				
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,2);	
		}
		struct server_braod_mx_data *ser_ip;
		ser_ip = (struct server_braod_mx_data *)buf;
		kprintf("report_seq = %d\n",ser_ip->report_seq);
		memcpy(g_srv_broad_mx,ser_ip,sizeof(struct server_braod_mx_data));
		sprintf(g_str_guid,"%s",ser_ip->guid);
		g_try_count = ser_ip->try_count;
		//printf("----------------->>> g_try_count= %d\n",g_try_count);
		
		memset(ack_play_buf,0,24);
		struct ip_play_ack_data *loc_play_ack;
		loc_play_ack = (struct ip_play_ack_data *)ack_play_buf;
		sprintf(loc_play_ack->cmd_id,"EMERGENCY_ACK");
		loc_play_ack->try_count = ser_ip->try_count;
		loc_play_ack->report_seq = ser_ip->report_seq;
		ack_reponse_to_server_by_socket_24(client_sockfd,ack_play_buf);
				
		char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
		sprintf(temp_log,"CFIL14,%s,%s,EMERGENCY MIC BROADCASTING REQUEST,%s",gMyCallNumber,g_str_guid,ser_ip->file_name);				
		udp_message_data_send_proc(temp_log,5);
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_EMERGENCY_PLAY);
		req_emergency_broadcast_msgq(ser_ip->file_name);
	}
	else{
		kprintf("CMD_ID : %s \n",loc_cmd->cmd_id);
	}
}


void Read_IP_File_List(char *s_sdata)
{
	struct file_list_ack_data *loc_ack;
	loc_ack = (struct file_list_ack_data *)s_sdata;

	struct  dirent **namelist;
    int     count;
    int     idx,loc_cnt=0;
	char *path = "/mmc/IP_ment";
	struct stat st;

    if((count = scandir(path, &namelist, NULL, alphasort)) == -1) 	
	{
        fprintf(stderr, "%s Directory Scan Error: %s\n", path, strerror(errno));
		loc_ack->num = 0;
        return;
    }

	for(idx = 0; idx < count; idx++) {		
		//if(strstr(namelist[idx]->d_name,".wav") != NULL)
		if((strcmp(namelist[idx]->d_name, ".") != 0) && (strcmp(namelist[idx]->d_name, "..") != 0))
		{
			sprintf(loc_ack->file_name[loc_cnt],"%s",namelist[idx]->d_name);
	        kprintf("%s\n", loc_ack->file_name[loc_cnt]);
			loc_cnt++;
		}
    }
	loc_ack->num = (unsigned char)loc_cnt;
	
	free(namelist);
}


#endif

void udp_message_data_send_proc(char *log_data, int cmd_id)
{
#ifndef USE_SUJAWON_DEF	
	if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
		if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet ==1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M ==1)){

/*
			unsigned char loc_cmd_check=0;
			if((g_lte_status == 1) && (cmd_id == 1)) loc_cmd_check = 1;
			else if((g_lte_log == 1) && (cmd_id == 2)) loc_cmd_check = 1;
			else if((g_lte_alarm == 1) && (cmd_id == 3)) loc_cmd_check = 1;
			else if((g_lte_warning == 1) && (cmd_id == 4)) loc_cmd_check = 1;
			else if((g_lte_command == 1) && (cmd_id == 5)) loc_cmd_check = 1;
			else return;

			if(loc_cmd_check == 0) return;
*/

			char udp_buf[TOT_IP_SEND_MSG_LEN];
			memset(udp_buf,0,TOT_IP_SEND_MSG_LEN);
			struct ip_report_message *snd_ip_data;
			snd_ip_data = (struct ip_report_message *)udp_buf;
			snd_ip_data->cmd_id = cmd_id;			
			sprintf(snd_ip_data->rpt_ptr.buf,"%s",log_data);	
			//printf("======>>> snd_ip_data->rpt_ptr.buf = %d\n",strlen(snd_ip_data->rpt_ptr.buf));
			udp_client_report(udp_buf);
		}
		else if(g_log_data_send_flag == 1){
			struct timeval loc_time_t;
			struct tm loc_time_tm;
			gettimeofday(&loc_time_t, NULL);
			localtime_r(&loc_time_t.tv_sec, &loc_time_tm);
		
			char udp_buf[TOT_IP_SEND_MSG_LEN];
			memset(udp_buf,0,TOT_IP_SEND_MSG_LEN);
			struct log_report_message *snd_ip_data;
			snd_ip_data = (struct log_report_message *)udp_buf;
			snd_ip_data->cmd_id = cmd_id;
			sprintf(snd_ip_data->buf,"%s",log_data); 
			sprintf(snd_ip_data->act_date,"%4d-%02d-%02d %02d:%02d:%02d",loc_time_tm.tm_year+1900,loc_time_tm.tm_mon+1,loc_time_tm.tm_mday\
				,loc_time_tm.tm_hour, loc_time_tm.tm_min, loc_time_tm.tm_sec);
			udp_client_report_data_save_to_file(udp_buf);
		}
	}
	else if((g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT) || (g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT)){
		if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet ==1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M ==1)){
	/*		
			unsigned char loc_cmd_check=0;
			if((g_lte_status == 1) && (cmd_id == 1)) loc_cmd_check = 1;
			else if((g_lte_log == 1) && (cmd_id == 2)) loc_cmd_check = 1;
			else if((g_lte_alarm == 1) && (cmd_id == 3)) loc_cmd_check = 1;
			else if((g_lte_warning == 1) && (cmd_id == 4)) loc_cmd_check = 1;
			else if((g_lte_command == 1) && (cmd_id == 5)) loc_cmd_check = 1;
			else return;

			if(loc_cmd_check == 0) return;
	*/		
			char udp_buf[TOT_IP_SEND_MSG_LEN];
			memset(udp_buf,0,TOT_IP_SEND_MSG_LEN);
			struct ip_report_message *snd_ip_data;
			snd_ip_data = (struct ip_report_message *)udp_buf;
			snd_ip_data->cmd_id = cmd_id;			
			sprintf(snd_ip_data->rpt_ptr.buf,"%s",log_data);		
			udp_client_report(udp_buf);
		}
		else if(g_log_data_send_flag == 1){
			struct timeval loc_time_t;
			struct tm loc_time_tm;
			gettimeofday(&loc_time_t, NULL);
			localtime_r(&loc_time_t.tv_sec, &loc_time_tm);
		
			char udp_buf[TOT_IP_SEND_MSG_LEN];
			memset(udp_buf,0,TOT_IP_SEND_MSG_LEN);
			struct log_report_message *snd_ip_data;
			snd_ip_data = (struct log_report_message *)udp_buf;
			snd_ip_data->cmd_id = cmd_id;
			sprintf(snd_ip_data->buf,"%s",log_data); 
			sprintf(snd_ip_data->act_date,"%4d-%02d-%02d %02d:%02d:%02d",loc_time_tm.tm_year+1900,loc_time_tm.tm_mon+1,loc_time_tm.tm_mday\
				,loc_time_tm.tm_hour, loc_time_tm.tm_min, loc_time_tm.tm_sec);
			send_udp_log_report_data(udp_buf);
		}
	}
#endif	
}

void req_rec_live_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];

	sprintf(loc_buf," rtmp://%s:1935/vod/mp3:%s",get_server_ip(),file_buf);
	kprintf("req_rec_live_broadcast_msgq()!!!! %s\n",loc_buf);
	set_broad_file_name(loc_buf);	
	sendBroadcastRecLIVEMsg(MSG_REC_LIVE_FILE_BOARDCAST_START, 0xFE, loc_buf);			
}

void req_live_broadcast_act_msgq()
{
	req_live_broadcast_msgq(g_srv_broad_mx->file_name);		
}


void req_live_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
	
	if(strstr(file_buf, ".mp4") != NULL){
		char *ptr = strstr(file_buf, ".mp4");
		int c_len =  (int)ptr - (int)file_buf;
		char lot_file_name[200];
		memset(lot_file_name,0,200);
		strncpy(lot_file_name,file_buf,c_len);
		sprintf(loc_buf,"%s",lot_file_name);
		//sprintf(loc_buf," rtsp://%s:1935/%s",get_server_ip(),lot_file_name);
	}
	else{
		sprintf(loc_buf,"live/myStream");
		//sprintf(loc_buf," rtsp://%s:1935/live/myStream",get_server_ip());
	}

	kprintf("req_live_broadcast_msgq()!!!! %s\n",loc_buf);
	set_broad_file_name(loc_buf);	
	sendBroadcastLIVEMsg(MSG_LIVE_VOICE_BOARDCAST_START, 0xFE, loc_buf);			
}

void req_emergency_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
	
	if(strstr(file_buf, ".mp4") != NULL){
		char *ptr = strstr(file_buf, ".mp4");
		int c_len =  (int)ptr - (int)file_buf;
		char lot_file_name[200];
		memset(lot_file_name,0,200);
		strncpy(lot_file_name,file_buf,c_len);
		sprintf(loc_buf,"%s",lot_file_name);
		//sprintf(loc_buf," rtsp://%s:1935/%s",get_server_ip(),lot_file_name);
	}
	else{
		sprintf(loc_buf,"live/myStream");
		//sprintf(loc_buf," rtsp://%s:1935/live/myStream",get_server_ip());
	}

	kprintf("req_live_broadcast_msgq()!!!! %s\n",loc_buf);
	set_broad_file_name(loc_buf);	
	sendBroadcastEMERGENCYMsg(MSG_EMERGENCY_PLAY_BOARDCAST_START, 0xFE, loc_buf);			
}

void req_reserve_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
		
	sprintf(loc_buf," rtmp://%s:1935/vod/mp3:%s",get_server_ip(),file_buf);
	kprintf("req_reserve_broadcast_msgq!!!! %s\n",loc_buf);
	set_broad_file_name(loc_buf);	
	sendBroadcastReserveMsg(MSG_RESERVE_PLAY_BOARDCAST_START, 0xFE, loc_buf);			
}

void req_tts_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
		
	sprintf(loc_buf," rtmp://%s:1935/vod/mp3:%s",get_server_ip(),file_buf);
	kprintf("req_tts_broadcast_msgq!!!! %s\n",loc_buf);
	set_broad_file_name(loc_buf);	
	sendBroadcastTTSMsg(MSG_TTS_PLAY_BOARDCAST_START, 0xFE, loc_buf);			
}

void req_siren_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
	
	sprintf(loc_buf,"/mmc/siren/%s",file_buf);
	set_broad_file_name(loc_buf);
	sendBroadcastIPSIRENMsg(MSG_SIREN_PLAY_BOARDCAST_START, 0xFE, loc_buf);			
}


void req_file_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
	
	sprintf(loc_buf,"/mmc/IP_ment/%s",file_buf);
	set_broad_file_name(loc_buf);
#ifdef DEF_IP_FILE_DIVIDE_USE	
	sendBroadcastMsgIPFilePlay(MSG_IP_FILE_PLAY_BOARDCAST_START, loc_buf, BROADCAST_GROUP_ALL, CK_USE, CK_USE);
#else
	sendBroadcastIPFILEMsg(MSG_FILE_PLAY_BOARDCAST_START, 0xFE, loc_buf);			
#endif
	
}

void file_list_ack_reponse_to_server_by_socket(int client_sockfd, char *ack_buf,int len)
{
	if(write(client_sockfd, ack_buf, len) <= 0)
	{
		kprintf("write error: %s\n",strerror(errno));
	}
}

void ack_reponse_to_server_by_socket(int client_sockfd, char *ack_buf)
{
	if(write(client_sockfd, ack_buf, SOC_MAXBUF) <= 0)
	{
		kprintf("write error: %s\n",strerror(errno));
	}
}

void ack_reponse_to_server_by_socket_24(int client_sockfd, char *ack_buf)
{
	if(write(client_sockfd, ack_buf, 24) <= 0)
	{
		kprintf("write error: %s\n",strerror(errno));
	}
}

void stop_currently_runing_service(int client_sockfd,int r_idx)
{
	char loc_buf[16];
	int loc_loop_count = 0;
	int loc_broadcast_state = getCurrentBroadcastState();
	int loc_r_idx = r_idx;

	memset(loc_buf,0,SOC_MAXBUF);
	struct ip_ack_data *loc_ack;
	loc_ack = (struct ip_ack_data *)loc_buf;
	sprintf(loc_ack->cmd_id,"READY_IND");

	kprintf("Current BroadCast State = %d\n",loc_broadcast_state);
	if(loc_broadcast_state != BOARDCAST_TYPE_NONE){		
		set_g_stop_proc_check_flag(0);
		if((get_check422() == 1) || (get_checkBCDMA_VAL() ==1)) r_idx = 0;
		
		if((loc_broadcast_state == BOARDCAST_STATE_FILE_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_FILE_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,FILE PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			if(get_IP_Req_Status_Index() ==BOARDCAST_IP_TYPE_NONE){
				ip_status_report_proc(6);				
			}
			g_pri_stop_flag = r_idx;
			sendBroadcastMsgFilePlay(MSG_FILE_PLAY_BOARDCAST_STOP, "none", BROADCAST_GROUP_ALL, CK_USE, CK_USE);	
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_TEL_READY) || (loc_broadcast_state == BOARDCAST_STATE_TEL_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,TELEPHONE PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			if(get_g_OAEPRO_STATE_flag() == 1) ip_status_report_proc(9);
			else ip_status_report_proc(8);
			g_pri_stop_flag = r_idx;
			//set_g_call_end_flag(0);
			cdma_EndCall();	
			//cdma_EndCall_direct();
			/*
			set_isConnecting(CK_OFF);   //swpark
			set_isBroadcastTel(CK_OFF); //swpark */
			//Tel_connection_disconnect();
			
			/*
			while((getCurrentBroadcastState() != BOARDCAST_STATE_NONE)	&&(loc_loop_count < 100))
			{
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}			*/
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);			
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_MIC_READY) || (loc_broadcast_state == BOARDCAST_STATE_MIC_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,MIC PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			ip_status_report_proc(5);
			g_pri_stop_flag = r_idx;
			sendMicBroadcastMsg(MSG_MIC_BOARDCAST_BOARDCAST_STOP,BROADCAST_GROUP_ALL , CK_USE, CK_USE);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_TTS_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_TTS_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,TTS PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			g_pri_stop_flag = r_idx;
			sendBroadcastMsgTTSPlay(MSG_TTS_PLAY_BOARDCAST_STOP,"none",BROADCAST_GROUP_ALL , CK_USE, CK_USE);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_LIVE_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_LIVE_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,LIVE PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			g_pri_stop_flag = r_idx;
			
			sendBroadcastMsgLIVEPlay(MSG_LIVE_VOICE_BOARDCAST_STOP,"none",BROADCAST_GROUP_ALL , CK_USE, CK_USE);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_REC_LIVE_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_REC_LIVE_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,SAVED LIVE FILE PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			g_pri_stop_flag = r_idx;
			
			sendBroadcastMsgRECLIVEPlay(MSG_REC_LIVE_FILE_BOARDCAST_STOP,"none",BROADCAST_GROUP_ALL , CK_USE, CK_USE);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_PSTN_READY) || (loc_broadcast_state == BOARDCAST_STATE_PSTN_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,PSTN",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			ip_status_report_proc(8);
			g_pri_stop_flag = r_idx;
			sendPSTNBroadcastMsg(MSG_PSTN_BOARDCAST_BOARDCAST_STOP, CK_USE, CK_NO_USE);
			set_g_PSTN_exit_flag(0);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_SUNCHA_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_SUNCHA_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,Suncha",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);	
			ip_status_report_proc(8);
			g_pri_stop_flag = r_idx;
			sendBroadcastSunChaStopMsg(MSG_FILE_PLAY_BOARDCAST_STOP,"none",BROADCAST_GROUP_NOTHING , CK_NO_USE, CK_NO_USE);			
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_SIREN_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_SIREN_PLAY_START) \
				|| (loc_broadcast_state == BOARDCAST_STATE_INTERNAL_FILE_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,Siren",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);	
			g_pri_stop_flag = r_idx;
			
			sendBroadcastMsgSirenPlay(MSG_SIREN_PLAY_BOARDCAST_STOP,"none",BROADCAST_GROUP_ALL , CK_USE, CK_USE);			
		}
		else if (loc_broadcast_state == BOARDCAST_STATE_SUDONG){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,SUDONG MIC",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			ip_status_report_proc(5);
			g_pri_stop_flag = r_idx;
			sendCheckSudongBroadcastMsg(MSG_MIC_BOARDCAST_BOARDCAST_STOP);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_RESERVE_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_RESERVE_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,Reserved Audio PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			g_pri_stop_flag = r_idx;
			sendBroadcastMsgReservedSoundPlay(MSG_RESERVE_PLAY_BOARDCAST_STOP,"none",BROADCAST_GROUP_ALL , CK_USE, CK_USE);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
		else if ((loc_broadcast_state == BOARDCAST_STATE_EMERGENCY_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_EMERGENCY_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,EMERGENCY PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			g_pri_stop_flag = r_idx;
			
			sendBroadcastMsgEmergencyPlay(MSG_EMERGENCY_PLAY_BOARDCAST_STOP,"none",BROADCAST_GROUP_ALL , CK_USE, CK_USE);
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
#ifdef DEF_IP_FILE_DIVIDE_USE		
		else if((loc_broadcast_state == BOARDCAST_STATE_IP_FILE_PLAY_READY) || (loc_broadcast_state == BOARDCAST_STATE_IP_FILE_PLAY_START)){
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"LSTP2,%s,%s,PRIORITY BROADCAST STOP,IP FILE PLAY",gMyCallNumber,g_str_guid);
			udp_message_data_send_proc(temp_log,2);
			
			g_pri_stop_flag = r_idx;
			sendBroadcastMsgIPFilePlay(MSG_IP_FILE_PLAY_BOARDCAST_STOP, "none", BROADCAST_GROUP_ALL, CK_USE, CK_USE);							
			while((get_g_stop_proc_check_flag() == 0) && (loc_loop_count < 100)){
				app_thr_wait_msecs(100);
				loc_loop_count++;
			}
			setCurrentBroadcastState(BOARDCAST_STATE_NONE);
		}
#endif		
		else if (loc_broadcast_state == BOARDCAST_STATE_LINE_IN_START){
		}		
	}	
	else g_pri_stop_flag = 0;
#ifndef USE_SUJAWON_DEF	
#if 0
	if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT)
	{
		if(client_sockfd != -1){
			if(write(client_sockfd, loc_buf, SOC_MAXBUF) <= 0)
			{
				perror("write error:");
			}
		}
	}
#endif
#endif	
}

int udp_client_report(char buf[TOT_IP_SEND_MSG_LEN])
{
#ifndef USE_SUJAWON_DEF	
	if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
		if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet ==1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M ==1))
		{
			send_udp_client_report_data(buf);
		}		
	}
	else if((g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT) || (g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT)){
		send_udp_client_report_data(buf);
	}
#endif
	return 1;
}

void udp_client_report_data_save_to_file(char *buf)
{
	int loc_fd = open("/mmc/log_report/saved_log_file", O_RDWR | O_CREAT | O_APPEND, 0666);
	if(loc_fd < 0){
		kprintf("[%s] /mmc/log_report/saved_log_file File Open Error!!!\n",__FUNCTION__);
		return;
	}
	//char loc_buf[TOT_IP_SEND_MSG_LEN]={0,};
	//sprintf(loc_buf,"%s\n",buf);
	//printf("[%s] save buf string length = %d\n",__FUNCTION__,sizeof(buf));
	//write(loc_fd,loc_buf,TOT_IP_SEND_MSG_LEN);
	write(loc_fd,buf,TOT_IP_SEND_MSG_LEN);
	close(loc_fd);
	system("sync");
}

int send_udp_client_report_data(char buf[TOT_IP_SEND_MSG_LEN])
{
	int udp_sock; //socket
    int nbyte;
	struct sockaddr_in servaddr;
	int addrlen = sizeof(servaddr); //서버 주소의 size를 저장
	
	kprintf("udp_client_report() get_server_ip() = %s\n",get_server_ip());
	if(strcmp(get_server_ip(),"0.0.0.0") == 0) 
	{
		kprintf("server IP not setting yet\n");
		return 0;
	}
	if(strcmp(gMyCallNumber,"") == 0)
	{
		kprintf("My Call Number is not setting yet!!!\n");
		return 0;
	}
    //socket 연결 0보다 작으면 Error
    if((udp_sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        kprintf("socket fail: %s",strerror(errno));
        return 0;
    }
    
    //서버 주소 구조
    memset(&servaddr, 0, addrlen); //bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(get_server_ip()); 
    servaddr.sin_port = htons(IP_UDP_PORT); 
    if((sendto(udp_sock, buf, TOT_IP_SEND_MSG_LEN, 0, (struct sockaddr *)&servaddr, addrlen)) < 0) {
        //perror("sendto fail");
		kprintf("sendto fail : %s\n",strerror(errno));
		close(udp_sock); //socket close
        return 0;
    }
    close(udp_sock); //socket close

	return 1;
}


int send_udp_log_report_data(char *buf)
{
	int udp_sock; //socket
    int nbyte;
	struct sockaddr_in servaddr;
	int addrlen = sizeof(servaddr); //서버 주소의 size를 저장
	
	kprintf("send_udp_log_report_data() get_server_ip() = %s\n",get_server_ip());
	if(strcmp(get_server_ip(),"0.0.0.0") == 0) 
	{
		kprintf("server IP not setting yet\n");
		return 0;
	}
	if(strcmp(gMyCallNumber,"") == 0)
	{
		kprintf("My Call Number is not setting yet!!!\n");
		return 0;
	}
    //socket 연결 0보다 작으면 Error
    if((udp_sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        kprintf("socket fail: %s",strerror(errno));
        return 0;
    }
    
    //서버 주소 구조
    memset(&servaddr, 0, addrlen); //bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(get_server_ip()); 
    servaddr.sin_port = htons(13499); 
    if((sendto(udp_sock, buf, TOT_IP_SEND_MSG_LEN, 0, (struct sockaddr *)&servaddr, addrlen)) < 0) {
        //perror("sendto fail");
		kprintf("sendto fail : %s\n",strerror(errno));
		close(udp_sock); //socket close
        return 0;
    }
    close(udp_sock); //socket close

	return 1;
}

	
struct server_braod_mx_data* get_g_srv_broad_mx()
{
	return (struct server_braod_mx_data *)g_srv_broad_mx;
}


int db_read_add_audio_file_to_local_file(char *filename)
{
	char loc_save_file_name[200];
	sprintf(loc_save_file_name,"/mmc/IP_ment/%s",filename);
	FILE *fpData = fopen(loc_save_file_name, "wb");
  
	if(fpData == NULL) {
		kprintf("[%s] %s\n", __FUNCTION__, "cannot open Data file");
		return -1;
	}

	MYSQL *con = mysql_init(NULL);
	char LogQuery[1000];

	if(con == NULL){
		kprintf("[%s] %s\n", __FUNCTION__, "mysql_init() failed");
		//mysql_close(con);
		return -1;
	}  

	if(mysql_real_connect(con, get_server_ip(), "root", "oae112", "ip_comm", 16033, NULL, 0) == NULL){
		if(mysql_real_connect(con, "175.126.43.162", "root", "oae112", "ip_comm", 16033, NULL, 0) == NULL){
		  	mysql_close(con);
			return -1;
		}
	}	
	
	sprintf(LogQuery, "select dataFile from ip_saved_sound where file_name='%s'",filename);	
	kprintf("[%s] LogQuery : %s\n", __FUNCTION__, LogQuery);
	
	if(mysql_query(con, LogQuery)){		
		kprintf("db_read_add_audio_file_to_local_file() update error = %s\n",mysql_error(con));
	  	mysql_close(con);
	  	return -1;
	}

	MYSQL_RES *result = mysql_store_result(con);

	if(result == NULL){
		kprintf("[%s] %s\n", __FUNCTION__, "DataFile mysql_store_result() failed");
	  	mysql_close(con);
		return -1;
	}  

	MYSQL_ROW row = mysql_fetch_row(result);
	unsigned long *lengths = mysql_fetch_lengths(result);

	if(lengths == NULL){
		kprintf("[%s] %s\n", __FUNCTION__, "DataFile mysql_store_result() lengths null");
	  	mysql_close(con);
		return -1;
	}

	fwrite(row[0], lengths[0], 1, fpData);

	if(ferror(fpData)){            
		kprintf("[%s] %s\n", __FUNCTION__, "fwrite() failed");
		mysql_free_result(result);
		mysql_close(con);
	  	return -1;     
	}  

	int r = fclose(fpData);

	if (r == EOF) {
		kprintf("[%s] %s\n", __FUNCTION__, "cannot close file handler");
	}
	system("sync");
	mysql_free_result(result);
	mysql_close(con);
	return 1;
}


int get_g_play_time()
{
	return g_play_time;
}

int g_mtm_server_cockfd=-1;

#ifndef USE_SUJAWON_DEF

int ENT_Socket_closed_proc(void *ptr)
{
	pthread_detach(pthread_self()); 

	if(g_mtm_server_cockfd != -1)
		close(g_mtm_server_cockfd);	
	//kprintf(".........................44444444444444444444444444444444444\n");
}

int socket_m2m_client_main()
{
	struct sockaddr_in serveraddr;
	//int server_sockfd;
	int client_len,iMode=0;
	fd_set readfds, tmp_fds;
	struct timeval tv; 
	
	kprintf("socket_m2m_client_main() waken!!!\n");
	int init_connect_socket_flag = 0,g_ip_timeout = 0;
#ifdef DEF_ENTIMORE_IP_USE	
	sleep(1);
#endif	
	while(1){
		if(g_cdma_reset_flag == 1) return;
		g_ip_timeout = 0;
		if(strcmp(get_server_ip(),"0.0.0.0") == 0) 
		{
			kprintf("[%s]server IP not setting yet\n",__FUNCTION__);
			sleep(60);
			continue;
		}
		//kprintf(".........................111111111111111111111111\n");
		if((g_mtm_server_cockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)	
		{
			kprintf("socket_m2m_client_main() : socket error : ");
			sleep(30);			
			continue;
		}
		ioctl(g_mtm_server_cockfd, FIONBIO, &iMode);
		tcflush(g_mtm_server_cockfd, TCIOFLUSH);
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = inet_addr(get_server_ip());
		serveraddr.sin_port = htons(13500);
		
		client_len = sizeof(serveraddr);
#ifdef DEF_ENTIMORE_IP_USE		
		if((g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT) && (g_ip_route_id == 1)){

			int enable = 1;
			if (setsockopt(g_mtm_server_cockfd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(int)) < 0)
				kprintf("setsockopt(SO_KEEPALIVE) failed\n");
			
			int time_e = 30;
			if (setsockopt(g_mtm_server_cockfd, SOL_TCP, TCP_KEEPIDLE, &time_e, sizeof(time_e)) < 0)
				kprintf("setsockopt(SO_KEEPIDLE) failed\n");

			int keep_cnt = 2;
			if (setsockopt(g_mtm_server_cockfd, SOL_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt)) < 0)
				kprintf("setsockopt(SO_KEEPIDLE) failed\n");

			int keep_intv = 30;
			if (setsockopt(g_mtm_server_cockfd, SOL_TCP, TCP_KEEPINTVL, &keep_intv, sizeof(keep_intv)) < 0)
				printf("setsockopt(SO_KEEPIDLE) failed\n");
			//kprintf(".........................222222222222222222222222222222\n");
		}
#endif		
		if(connect(g_mtm_server_cockfd, (struct sockaddr *)&serveraddr, client_len) == -1)
		{
			kprintf("socket_m2m_client_main() : connect error : \n");
			close(g_mtm_server_cockfd);		
			g_mtm_server_cockfd = -1;
			sleep(5);			
			continue;
		}
		//kprintf(".........................33333333333333333333333333\n");
		FD_ZERO(&readfds);
		FD_SET(g_mtm_server_cockfd, &readfds);

		thread_delete(_tobjM2mIpRcvThread);
		if(thread_create(_tobjM2mIpRcvThread, (void *) &M2mIpReg_Send_Proc, APP_THREAD_PRI, NULL) < 0) {
				eprintf("create thread\n");
		}
		
		while(1){			
			char locbuf[SOC_MAXRECVBUF];
			memset(locbuf, 0x00, SOC_MAXRECVBUF);
			int r_len=0;

			int loc_flag = 1;
			tmp_fds = readfds;
#ifdef DEF_ENTIMORE_IP_USE		
			if((g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT) && (g_ip_route_id == 1)){				
		        tv.tv_sec = g_ip_report_time+5;
		        tv.tv_usec = 0;
			}
			else{				
		        tv.tv_sec = g_ip_report_time+1;
		        tv.tv_usec = 0;
			}
#else
			 tv.tv_sec = g_ip_report_time+1;
		     tv.tv_usec = 0;
#endif
	        int loc_state = select(g_mtm_server_cockfd + 1, &tmp_fds, (fd_set *)0, (fd_set *)0, &tv);
	        switch(loc_state)
	        {
	            case -1:
	                kprintf("[%s] select error : \n",__FUNCTION__);
					if(g_mtm_server_cockfd != -1)
						close(g_mtm_server_cockfd);
					loc_flag = 1;					
	                break;    
	            case 0:
	                kprintf("[%s] Time over\n",__FUNCTION__); 
					if(g_mtm_server_cockfd == -1)
	                	loc_flag = 1;	
					else loc_flag = 2;	
					g_ip_timeout++;
	                break;
	            default:
					FD_ISSET(g_mtm_server_cockfd,&tmp_fds);
	                if((r_len = read(g_mtm_server_cockfd, locbuf, SOC_MAXRECVBUF)) <= 0)
					{
						kprintf("[%s] read error : \n",__FUNCTION__);
						loc_flag = 2;
						g_ip_timeout++;
					}
					else{
						loc_flag = 0;
					}
	                break;
	        }
			if(loc_flag == 1) break;
			else if(loc_flag == 2){
				if(g_ip_timeout < 1) continue;
				else break;
			}			
			Recieve_Command_from_IP_Server_process(g_mtm_server_cockfd, locbuf);	
		}
		thread_delete(_tobjM2mIpRcvThread);
#ifdef DEF_ENTIMORE_IP_USE		
		if((g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT) && (g_ip_route_id == 1)){
			pthread_t soc_thread_t;
			if (pthread_create(&soc_thread_t, NULL, &ENT_Socket_closed_proc, (void *)NULL) < 0)
			{
				kprintf("thread create error:\n");
			}
			app_thr_wait_msecs(100);
		}
		else{
			if(g_mtm_server_cockfd != -1)
				close(g_mtm_server_cockfd);	
		}
#else		
		if(g_mtm_server_cockfd != -1)
			close(g_mtm_server_cockfd);	
#endif		
		g_mtm_server_cockfd = -1;
	}
	return 0;
}

void M2mIpReg_Send_Proc()
{
	char locbuf[SOC_REGLEN];
	
	int init_client_reg_ip = -1,w_len=0,init_report= 0;
	kprintf("start : gMyCallNumber = %s\n",gMyCallNumber);

	if(strcmp(gMyCallNumber,"") == 0)
	{
		init_client_reg_ip = 0;
		kprintf("My Call Number is not setting yet!!!\n");
		read_IP_ID_value_to_gMyCallNumber_no_check();
	}
	while(1){
		if(init_client_reg_ip == 1){
			sleep(g_ip_report_time); 			
		}
		else if(init_client_reg_ip == 0){
			init_client_reg_ip = 1;
			sleep(5);			
		}
		
		//if(gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M ==1)
		{
			memset(locbuf, 0x00, SOC_REGLEN);	
			struct ip_req_data *loc_req = (struct ip_req_data *) locbuf;
			sprintf(loc_req->cmd_id,"REG_IP");
			loc_req->from_id = 0x03;
			loc_req->port_id = 13500;
			
			strcpy(loc_req->tel_num,gMyCallNumber);
			int i=0;
			kprintf("IP Registaration to Server\n");
			
			if((w_len = write(g_mtm_server_cockfd, locbuf, SOC_REGLEN)) <= 0)
			{
				kprintf("[%s] write error: %s\n",__FUNCTION__,strerror(errno));
				init_client_reg_ip = 0;
				continue;
			}	
			init_client_reg_ip = 1;
			
			char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
			sprintf(temp_log,"STMN1,%s,NULL,IP REGISTRATION REQUEST",gMyCallNumber);
			udp_message_data_send_proc(temp_log,1);		
	
			if(init_report == 0){
				ip_status_report_proc(3);
				init_report = 1;
			}
		}
	}
}

#endif

int get_g_live_client_sockfd()
{
	return g_live_client_sockfd;
}

void set_g_live_client_sockfd(int loc_fd)
{
	g_live_client_sockfd = loc_fd;
}

char* get_g_str_guid()
{
	return g_str_guid;
}

int get_g_try_count()
{
	return g_try_count;
}

int get_g_rec_pre_time()
{
	return g_rec_pre_time;
}


void ip_status_report_proc(int t_status)
{

	g_t_status = t_status;
	kprintf("ip_status_report_proc() t_status= %d\n",t_status);
#ifndef USE_SUJAWON_DEF		
	if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet ==1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M ==1)){
		sendServerIpStatusMsg(t_status);
	}
#endif  //USE_SUJAWON_DEF	
}


void socket_ip_status_report_thread_proc()
{
	struct sockaddr_in serveraddr;
	int server_sockfd;
	int client_len;
	char buf[SOC_REGLEN];
	kprintf("socket_ip_status_report_thread_proc() t_status = %d!!!\n",g_t_status);
	int init_client_reg_ip = 0;

		g_ip_sts_flag = 1;
	//if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet ==1)
	{
		if(strcmp(get_server_ip(),"0.0.0.0") == 0) 
		{
			kprintf("server IP not setting yet\n");
			return;
		}
		if(strcmp(gMyCallNumber,"") == 0)
		{
			init_client_reg_ip = 0;
			kprintf("My Call Number is not setting yet!!!\n");
			return;
		}
		if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)	
		{
			kprintf("error : %s\n",strerror(errno));
			return;
		}
		//printf("get_server_ip == %s\n",get_server_ip());
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = inet_addr(get_server_ip());
		serveraddr.sin_port = htons(13500);
		
		client_len = sizeof(serveraddr);
		
		if(connect(server_sockfd, (struct sockaddr *)&serveraddr, client_len) == -1)
		{
			kprintf("connect error : %s\n",strerror(errno));
			close(server_sockfd);
			return;
		}

		//printf("----------------------------------------------------------------------->>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		memset(buf, 0x00, SOC_REGLEN);	
		struct ip_status_report *loc_req = (struct ip_status_report *) buf;
		sprintf(loc_req->cmd_id,"STATUS_REPORT");
		loc_req->from_id = 0x02;
		loc_req->status= (char)g_t_status;
		
		strcpy(loc_req->tel_num,gMyCallNumber);
		
		if(write(server_sockfd, buf, SOC_REGLEN) <= 0)
		{
			kprintf("write error: %s\n",strerror(errno));
			close(server_sockfd);
			return;
		}	
		//printf("socket_ip_status_report_thread_proc() server write OK!!\n");
/*		
		memset(buf, 0x00, SOC_REGLEN);
		int r_len=0;
		alarm(2);
		if((r_len = read(server_sockfd, buf, SOC_REGLEN)) <= 0)
		{
			perror("read error : ");
			close(server_sockfd);
			return;
		}
		alarm(0);
*/
		close(server_sockfd);	
		g_ip_sts_flag = 0;
	}	
}

int get_CFUN_flag()
{
	return CFUN_flag;
}

void set_CFUN_flag(int loc_flag)
{
	CFUN_flag = loc_flag;
}

	

int IpStsTfunction()
{
	IpStsID = msgget((key_t)MSGIPSTSQ, IPC_CREAT|0666);
	if(IpStsID == -1) {
		kprintf("message queue MSGIPSTSQ not created!! errno = %s \n",strerror (errno));
		return -1;
	}
	
	kprintf("[NAW] Start IP Status Report Thread\n");
	
	while(1){
		IpStsMsgTypeDef rcv_ipstsQ;
		memset(&rcv_ipstsQ,0,sizeof(rcv_ipstsQ));
		
		if(-1 == msgrcv(IpStsID, &rcv_ipstsQ, sizeof(rcv_ipstsQ) - sizeof(long), 0, 0))
		{
			kprintf( "IPstatus msgrcv failed");
			continue; 
		}

		g_t_status = rcv_ipstsQ.messageType;
		socket_ip_status_report_thread_proc();		
	}	
	return 1;
}

void sendServerIpStatusMsg(int messageType)
{
	int msgid;
	IpStsMsgTypeDef snd_ipstsQ;

	//kprintf("sizeof(snd_ipstsQ) = %d\n",sizeof(snd_ipstsQ));

	memset(&snd_ipstsQ, 0, sizeof(snd_ipstsQ));
	
	snd_ipstsQ.mtype = MSG_TYPE_IPSTSQ;
	snd_ipstsQ.messageType = messageType;
	
	msgid = msgsnd(IpStsID, &snd_ipstsQ, sizeof(snd_ipstsQ) - sizeof(long), 0);
	if(msgid == -1)
		kprintf( "sendServerIpStatusMsg Send Msg failed");
}

int get_g_pri_stop_flag()
{
	return g_pri_stop_flag;
}

void set_g_pri_stop_flag(int loc_flag)
{
	g_pri_stop_flag = loc_flag;
}

/* Auth Init */
void InitAuth(void)
{
	struct timeval t_time;
	gettimeofday(&t_time,NULL);
	srand((unsigned int)t_time.tv_usec);
}


#ifdef USE_SUJAWON_DEF
int Sujawon_server_main()
{	
	int server_sockfd, client_sockfd;
	int client_len, n;
	char buf[SOC_MAXRECVBUF];
	struct sockaddr_in clientaddr, serveraddr;
	pid_t pid;
	kprintf("Sujawon_server_main() waken!!!\n");
	
	if((server_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		kprintf("socket error: %s\n",strerror(errno));
		return 1;
	}
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
#if 1	
	serveraddr.sin_port = htons(get_g_server_ipcomm_port());
#else	
	serveraddr.sin_port = htons(12360);
#endif	
	int enable = 1;
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		kprintf("setsockopt(SO_REUSEADDR) failed");
	
	bind(server_sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	listen(server_sockfd, 1024);
	client_len = sizeof( clientaddr);
	while(1)
	{
		memset(buf, 0x00, SOC_MAXRECVBUF);
		client_sockfd = accept(server_sockfd, (struct sockaddr *)&clientaddr,(socklen_t*)&client_len);
		kprintf("New Client Connect : %s\n", inet_ntoa(clientaddr.sin_addr));
		sprintf(g_suja_ser_ip,"%s",inet_ntoa(clientaddr.sin_addr));
		if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1)
		{
			if((n = read(client_sockfd, buf, SOC_MAXRECVBUF)) <= 0)
			{
				close(client_sockfd);
				kprintf("MX400 sever receive read data error: %s\n",,strerror(errno));
				continue;
			}
			Sujawon_Recieve_Command_process(client_sockfd, buf);
		}
		if(g_live_client_sockfd == -1){
			close(client_sockfd);	
		}	
	}
	close(server_sockfd);	
	return 0;
}

void Sujawon_Recieve_Command_process(int client_sockfd, char *buf)
{
	char ack_buf[SOC_MAXBUF];
	struct suja_cmd_data *loc_cmd;
	loc_cmd = (struct suja_cmd_data *)buf;
	int i=0;
	kprintf("server receive data : %s\n",loc_cmd->cmd_id);
	
	if(strcmp(loc_cmd->cmd_id,"STOP_CMD") == 0)
	{
		struct stop_req_mx_data *loc_stop;
		loc_stop = (struct stop_req_mx_data *)buf;
		set_g_SunCha_Mode(CK_OFF);
		sprintf(g_str_guid,"%s",loc_stop->guid);
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"STOP_ACK");
		suja_ack_reponse_to_server_by_socket(client_sockfd,ack_buf,sizeof(ack_buf));
		
		g_play_time = 0;
		g_rcv_flag_from_ip_server = 1;
		stop_currently_runing_service(client_sockfd,0);	 
		g_live_client_sockfd = -1;
	}
	else if(strcmp(loc_cmd->cmd_id,"STATUS_REQ") == 0)
	{			
		char sts_buf[117];
		
		memset(sts_buf, 0x00, sizeof(sts_buf));	
		struct suja_status_ack *suja_loc_ack = (struct suja_status_ack *) sts_buf;
		sprintf(suja_loc_ack->cmd_id,"STATUS_REPORT");
		if(g_t_status == 3) sprintf(suja_loc_ack->result,"IDLE STATE");
		else if(g_t_status == 1) sprintf(suja_loc_ack->result,"BROADCATING READY STATE");
		else if(g_t_status == 55) sprintf(suja_loc_ack->result,"MIC BROADCATING STATE");
		else if(g_t_status == 56) sprintf(suja_loc_ack->result,"FILE BROADCATING STATE");
		else if(g_t_status == 58) sprintf(suja_loc_ack->result,"TELEPHONE BROADCATING STATE");
		else if(g_t_status == 59) sprintf(suja_loc_ack->result,"OAEPRO BROADCATING STATE");
		else if(g_t_status == 57) sprintf(suja_loc_ack->result,"SIREN BROADCATING STATE");
		else if(g_t_status == 2) sprintf(suja_loc_ack->result,"IP BROADCATING STATE");
		else sprintf(suja_loc_ack->result,"IDLE STATE");
		suja_ack_reponse_to_server_by_socket(client_sockfd,sts_buf,sizeof(sts_buf));		
	}
	else if(strcmp(loc_cmd->cmd_id,"FILE_BROAD_CMD") == 0)
	{
		set_g_SunCha_Mode(CK_OFF);
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"FILE_BROAD_ACK");
		suja_ack_reponse_to_server_by_socket(client_sockfd,ack_buf,sizeof(ack_buf));
		
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,0);
		}
		
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_FILE_PLAY);
		struct suja_file_broad_data *loc_suja_file;
		loc_suja_file = (struct suja_file_broad_data *)buf;
		suja_req_file_broadcast_msgq(loc_suja_file->file_name);  
	}
	else if(strcmp(loc_cmd->cmd_id,"TTS_BROAD_CMD") == 0)
	{			
		set_g_SunCha_Mode(CK_OFF);
		memset(ack_buf,0,SOC_MAXBUF);
		struct suja_tts_broad_ack *loc_ack;
		loc_ack = (struct suja_tts_broad_ack *) ack_buf;
		sprintf(loc_ack->cmd_id,"TTS_BROAD_ACK");
		suja_ack_reponse_to_server_by_socket(client_sockfd,ack_buf,sizeof(ack_buf));
		
		if(getCurrentBroadcastState() != BOARDCAST_STATE_NONE){
			g_rcv_flag_from_ip_server = 1;
			stop_currently_runing_service(client_sockfd,0);
		}
		
		set_IP_Req_Status_Index(BOARDCAST_IP_TYPE_TTS_PLAY);
		struct suja_tts_broad_data *loc_suja_tts;
		loc_suja_tts =  (struct suja_tts_broad_data *)buf;
		ftp_clientmain (loc_suja_tts->ip_addr,loc_suja_tts->port_id, loc_suja_tts->user_name, loc_suja_tts->passwd, loc_suja_tts->server_addr_dir, loc_suja_tts->file_name, "/mmc/file_down");
		suja_req_tts_broadcast_msgq(loc_suja_tts->file_name);  
	}	
	else if(strcmp(loc_cmd->cmd_id,"RMT_RST_CMD") == 0)
	{			
		memset(ack_buf,0,SOC_MAXBUF);
		struct ip_ack_data *loc_ack;
		loc_ack = (struct ip_ack_data *) ack_buf;
		sprintf(loc_ack->cmd_id,"RMT_RST_ACK");
		ack_reponse_to_server_by_socket(client_sockfd,ack_buf);

		rebootSystemCDMA(0);
	}
	else{
		kprintf("CMD_ID : %s \n",loc_cmd->cmd_id);
	}
}

void suja_req_file_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
	
	sprintf(loc_buf,"/mmc/IP_ment/%s",file_buf);
	set_broad_file_name(loc_buf);
	sendBroadcastIPFILEMsg(MSG_FILE_PLAY_BOARDCAST_START, 0xFE, loc_buf);			
}

void suja_req_tts_broadcast_msgq(char *file_buf)
{
	char loc_buf[250];
		
	sprintf(loc_buf,"/mmc/file_down/%s",file_buf);	
	kprintf("suja_req_tts_broadcast_msgq!!!! %s\n",loc_buf);
	set_broad_file_name(loc_buf);	
	sendBroadcastTTSMsg(MSG_TTS_PLAY_BOARDCAST_START, 0xFE, loc_buf);			
}

void suja_ack_reponse_to_server_by_socket(int client_sockfd, char *ack_buf,int len)
{
	if(write(client_sockfd, ack_buf, len) <= 0)
	{
		kprintf("write error: %s\n",strerror(errno));
	}
}

void suja_send_to_server_ip_result_data_proc(char *data,int len)
{
	struct sockaddr_in serveraddr;
	int server_sockfd;
	int client_len;
	
	kprintf("[%s] len = %d!!!\n",__FUNCTION__,len);
	int init_client_reg_ip = 0;

	if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet ==1)
	{
#if 1	//for test
		if(strcmp(get_server_ip(),"0.0.0.0") == 0) 
		{
			kprintf("suja_send_to_server_ip_result_data_proc() server IP not setting yet\n");
			return;
		}
		if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)	
		{
			kprintf("socket error : %s\n",strerror(errno));
			return;
		}
		serveraddr.sin_family = AF_INET;
#if 1		
		serveraddr.sin_addr.s_addr = inet_addr(get_server_ip());
#else
		serveraddr.sin_addr.s_addr = inet_addr(g_suja_ser_ip);
#endif
		serveraddr.sin_port = htons(get_g_server_ipcomm_port());	
		client_len = sizeof(serveraddr);
		
		if(connect(server_sockfd, (struct sockaddr *)&serveraddr, client_len) == -1)
		{
			kprintf("connect error : %s\n",strerror(errno));
			close(server_sockfd);
			return;
		}
				
		if(write(server_sockfd, data, len) <= 0)
		{
			kprintf("write error: %s\n",strerror(errno));
			close(server_sockfd);
			return;
		}			
		close(server_sockfd);	
#endif		
	}	
}
#endif

//IP 정보 가져오기
int get_system_MyIPAddress(char *ip_addr,char *nic_name)
{
	int sock;
	struct ifreq ifr;
	struct sockaddr_in *sin;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) 
	{
		kprintf("socket error: %s\n",strerror(errno));
		return 0;
	}

	strcpy(ifr.ifr_name, nic_name);

	if(strcmp(nic_name,"eth0") == 0){
		if (ioctl(sock, SIOCGIFFLAGS, &ifr)< 0)    
		{
			close(sock);
			return 0;
		}
		if(((ifr.ifr_flags & IFF_UP) == IFF_UP) && ((ifr.ifr_flags & IFF_RUNNING) == IFF_RUNNING))
		{
			 //printf("[%s] IF STATUS UP\n",nic_name); 
			 ;
		} 
		else { 
			//printf("[%s] IF STATUS DOWN\n",nic_name); 
			close(sock);
			return 0;
		}	
	}
	if (ioctl(sock, SIOCGIFADDR, &ifr)< 0)    
	{
		close(sock);
		return 0;
	}

	sin = (struct sockaddr_in*)&ifr.ifr_addr;
	strcpy(ip_addr, inet_ntoa(sin->sin_addr));
	
	close(sock);

	return 1;
}

#ifdef DEF_FOR_TEST_TIMER
void for_test_timer_print()
{
	kprintf("[%s] timer expired!!!\n",__FUNCTION__);
}

void for_test_timer_print_once()
{
	kprintf("[%s] timer expired!!!\n",__FUNCTION__);
}

#endif

#ifndef DEF_SYSTEM_TIMER_THREAD_USE
void timerHandler( int sig, siginfo_t *si, void *uc )
#else
void timerHandler( union sigval si )
#endif
{
#if 1
    timer_t *tidp;
#ifndef DEF_SYSTEM_TIMER_THREAD_USE
    tidp = si->si_value.sival_ptr;
#else
	tidp = si.sival_ptr;
#endif

    if ( *tidp == g_wtd_s_timer ){
		//kprintf("Watchdog TimerID\n");
#ifdef DEF_WATCHDOG_SYS_TIME_USE         
		thread_delete(_tobjThreadWatchdog_setting);
		if(thread_create(_tobjThreadWatchdog_setting, &setWDT_KeepAlive, APP_THREAD_PRI, NULL) < 0) {
			eprintf("Modem_reset_n_pwr_off_on create thread\n");
		}
#else		
		setWDT_KeepAlive();
#endif
    }
    else if ( *tidp == g_modem_s_timer ){
        kprintf("secondTimerID\n");
		thread_delete(_tobjThreadModemCheck_init);
		if(thread_create(_tobjThreadModemCheck_init, &for_modem_init_timer_proc, APP_THREAD_PRI, NULL) < 0) {
			eprintf("Modem_reset_n_pwr_off_on create thread\n");
		}
    }
#else
	thread_delete(_tobjThreadWatchdog_setting);
	if(thread_create(_tobjThreadWatchdog_setting, &setWDT_KeepAlive, APP_THREAD_PRI, NULL) < 0) {
		eprintf("Modem_reset_n_pwr_off_on create thread\n");
	}
#endif	
	/*
    else if ( *tidp == thirdTimerID )
        printf("thirdTimerID\n\n");
        */
        
}


#ifdef DEF_MODEM_RESTART_NEW_PROC
int checking_modem_restart_count()
{
	int ret_val = 0;
	if(access("/mmc/modem_rst_count", F_OK) == 0){
		//system("rm /mmc/modem_rst_count");
		unlink("/mmc/modem_rst_count");
	}
	system("fw_printenv | grep modem_restart_count > /mmc/modem_rst_count"); 

	char ver_char[200];
	int loc_fd,r_len=0,ii,ij;

	memset(ver_char,0,sizeof(ver_char));				
	if( 0 < ( loc_fd = open( "/mmc/modem_rst_count", O_RDONLY))){
	   	r_len = read(loc_fd, ver_char, 128);
	   	if(r_len == 0){
			ret_val = 0;
			kprintf("checking_modem_restart_count() modem_restart_count = 0 setting!!\n");
	   	}
	   	else{
			if(strstr(ver_char, "modem_restart_count") != NULL){
				char *ptr_c = strstr(ver_char, "modem_restart_count");
				char loc_char = *(ptr_c+ 20);
				ret_val = loc_char - 0x30;
			}
	   	}
	   	close(loc_fd);
    }
	return ret_val;
}

void rst_count_add()
{
	char sTmp[50];			
	
	sprintf(sTmp, "fw_setenv modem_restart_count %d",g_mdm_rst_count);
	system(sTmp);
	system("fw_setenv saveenv");
	system("sync");
}

void for_modem_init_timer_proc()
{
	kprintf("[%s] timer expired!!!\n",__FUNCTION__);
	
	if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
		if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) modem_cdma_init(1);
		else modem_cdma_init(0);
	}	
	else if(g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT){
		Telradin_modem_init();
	}	
	else if(g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT){
		mtom_modem_init();
		
	}
	if(CFUN_flag == 0){
		StopTimer_sys(g_modem_s_timer);
		sleep(1);
		startThreadTelSMS(CK_ON);
		g_mdm_rst_count = 0;
		rst_count_add();
		if(g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT){
			ip_link_check_route_setting(0,DEF_TELADIN_GW,DEF_TELADIN_IP_DEVICE);
		}
		else if(g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT){
			sleep(1);
			char loc_ip_addr[50];
			int loc_ip_ret_ppp0 = get_system_MyIPAddress(loc_ip_addr,DEF_MTOM_IP_DEVICE);
			kprintf("for_modem_init_timer_proc() loc_ip_ret_ppp0 = %d\n",loc_ip_ret_ppp0);
			if(loc_ip_ret_ppp0 == 0)
				system("/usr/local/bin/quectel-pppd.sh");
			ip_link_check_route_setting(0,DEF_MTOM_GW,DEF_MTOM_IP_DEVICE);
		}
#ifdef DEF_ENTIMORE_IP_USE		
		if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
			sleep(1);
			char loc_ip_addr[50];
			int loc_ip_ret_ppp0 = get_system_MyIPAddress(loc_ip_addr,DEF_ENTIMORE_IP_DEVICE);
			kprintf("for_modem_init_timer_proc() loc_ip_ret_ppp0 = %d\n",loc_ip_ret_ppp0);
			if(loc_ip_ret_ppp0 == 0) setPPP();
			ip_link_check_route_setting(0,DEF_ENTIMORE_GW,DEF_ENTIMORE_IP_DEVICE);
		}
#endif		
	}
	else{
		CFUN_flag = -1;
		Modem_reset_n_pwr_off_on();
	}
}

void start_modem_reset_n_check(int time_modem)
{
#ifndef DEF_SYSTEM_TIMER_THREAD_USE
	StartTimer_sys(&g_modem_s_timer,&for_modem_init_timer_proc,time_modem);		// 5분마다 모뎀 체크를 위한 timer 구동 , 5분 후 동작 
#else
	StartTimer_sys(&g_modem_s_timer,&for_modem_init_timer_proc_handle,time_modem);		// 5분마다 모뎀 체크를 위한 timer 구동 , 5분 후 동작 
#endif	
	thread_delete(_tobjThreadRSTModem);
	if(thread_create(_tobjThreadRSTModem, &Modem_reset_n_pwr_off_on, APP_THREAD_PRI, NULL) < 0) {
		eprintf("Modem_reset_n_pwr_off_on create thread\n");
	}
}
#endif

#ifdef DEF_NET_TIME_USE
void net_time_setting()
{
	int rr = getRdateState();
	kprintf("rr = %d\n",rr);
	if(rr == 1){
		g_log_print_flag = 0;
		char  time_buff[200];
		sprintf(time_buff,"rdate -s %s",BORA_NET);
		printf("net_time_setting() time_buff= %s\n",time_buff);
		system(time_buff);

		struct tm* local_timeinfo;
		time_t local_time = time(NULL) + (9 * 3600);
		
		local_timeinfo = localtime(&local_time);
		
		printf("local time and date : %s\n", asctime(local_timeinfo)); 
		memset(time_buff,0,sizeof(time_buff));
		sprintf(time_buff, "hwclock --set --date %04d%02d%02d%02d%02d.%d", local_timeinfo->tm_year+1900, local_timeinfo->tm_mon+1, local_timeinfo->tm_mday,\
			local_timeinfo->tm_hour, local_timeinfo->tm_min, local_timeinfo->tm_sec);
		printf("time_buff = %s\n", time_buff);
		
		system(time_buff);
		sprintf(time_buff, "hwclock -s");
		system(time_buff);
		g_log_print_flag = 1;
	}
}

int getRdateState()
{	
	char buffer[200];
	FILE *fp;
	
	int ret = 0;

	sprintf(buffer,"rdate -p %s", BORA_NET);
	fp = popen(buffer, "r");
	if(NULL == fp){
		kprintf("[%s] %s\n", __FUNCTION__, "popen() fail");
		return -1;
	}
	memset(buffer,0,sizeof(buffer));
	while(fgets( buffer, 200, fp)){
		kprintf("[%s] %s\n", __FUNCTION__, buffer);
		if(strstr(buffer, "timeout") == NULL){
			ret = 1;
			break;
		}		
	}

	pclose(fp); 
	
    if(ret == 1){
    	kprintf("[%s] %s\n", __FUNCTION__, "check OK !!!");
    }
	else{
		kprintf("[%s] %s\n", __FUNCTION__, "rdate ack NOK !!!");
	}
    
    return ret;
}
#endif

#ifndef DEF_WATCHDOG_SYS_TIME_USE
void watchdog_report_thraead()
{
	while(1){
		setWDT_KeepAlive();
#ifdef DEF_WATCHDOG_TIME_TEST
		sleep(3);
#else
		app_thr_wait_msecs(WATCHDOG_TIME_INTERVAL*1000);
#endif		
	}
}
#endif

int read_eth_link_check_processing()
{
	int ret_val = 0;
	if(access("/sys/class/net/eth0/operstate", F_OK) != 0){		
		return -1;
	}
	
	char ver_char[50];
	int loc_fd,r_len=0,ii,ij;

	memset(ver_char,0,sizeof(ver_char));				
	if( 0 < ( loc_fd = open( "/sys/class/net/eth0/operstate", O_RDONLY))){
	   	r_len = read(loc_fd, ver_char, sizeof(ver_char));
	   	if(r_len == 0){
			ret_val = 0;
			kprintf("ethernet up/down link info not found!!\n");
	   	}
	   	else{
			if(strstr(ver_char, "up") != NULL){
				ret_val = 1;
			}
			else ret_val = 0;
	   	}
	   	close(loc_fd);
    }
	return ret_val;
}

#ifdef DEF_ENTIMORE_IP_USE
int ethernet_link_connect_check()
{
	char loc_ip_addr[100];
	int loc_link_up_down_ret=0;
	
	memset(loc_ip_addr,0,sizeof(loc_ip_addr));
	int loc_ip_ret_eth0 = get_system_MyIPAddress(loc_ip_addr,"eth0");
	if(loc_ip_ret_eth0 == 1){
		loc_link_up_down_ret = read_eth_link_check_processing();
		if(loc_link_up_down_ret == 1) return 1;
		else return 0;
	}
	else {
		return 0;
	}
	return 0;	
}

#endif

void ip_link_check_route_setting(unsigned char id,char *gw_address, char *device_name)
{
	char loc_ip_addr[100];
	int loc_link_up_down_ret=0;
	static int tun_set_flag = CK_NO_USE; //vpn run flag
	static unsigned char g_ip_link_dual_check_flag = 0,st_enti_dual_gw_check_flag=0;

	if(g_ip_link_dual_check_flag == 1) return;
	g_ip_link_dual_check_flag = 1;
	memset(loc_ip_addr,0,sizeof(loc_ip_addr));
	int loc_ip_ret_eth0 = get_system_MyIPAddress(loc_ip_addr,"eth0");
	//printf("=======>>>>> loc_ip_ret_eth0= %d, g_ip_route_id= %d\n",loc_ip_ret_eth0,g_ip_route_id);
	if(loc_ip_ret_eth0 == 1){
		if(id == 0) kprintf("My system eth0 IP Address= %s\n",loc_ip_addr);
	}
	else {
		if(id == 0) kprintf("My system eth0 IP Address is not setting\n");
	}

	if(loc_ip_ret_eth0 == 1){
		loc_link_up_down_ret = read_eth_link_check_processing();		
	}
	int loc_ip_ret_eth1 = get_system_MyIPAddress(loc_ip_addr,device_name);
	if(loc_ip_ret_eth1 == 1){
		if(id == 0) kprintf("My system %s IP Address= %s\n",device_name,loc_ip_addr);
		if((loc_ip_ret_eth0 == 1) && (loc_link_up_down_ret == 1)){
			if(id == 0){
				modem_system_route_delete(gw_address,device_name);
				eth0_system_route_add();
				g_ip_route_id = 0;
				g_route_chg_flag = 1;
				kprintf("Setting Route table g_ip_route_id eth0\n");
			}
			else if(g_ip_route_id == 1){
				modem_system_route_delete(gw_address,device_name);
				if(id == 1){
					eth0_system_route_add();
					g_route_chg_flag = 1;
					if(g_vpn_config_data.vpn_use_flag == CK_USE){
						int loc_pid = get_pid_by_process_name("sgvpn_client");
						if(loc_pid > 0){
							char str_loc_pid[100]={0,};
							sprintf(str_loc_pid,"kill -9 %d",loc_pid);
							system(str_loc_pid);
							tun_set_flag = CK_NO_USE;
						}
					}
				}
				g_ip_route_id = 0;
				g_route_chg_flag = 1;
				kprintf("Change Route table g_ip_route_id %s -> eth0\n",device_name);
		
				if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M == 1)){
					if(g_mtm_server_cockfd != -1)
						close(g_mtm_server_cockfd);	
					thread_delete(_tobjM2mIpSndThread);
					if(thread_create(_tobjM2mIpSndThread, (void *) &socket_m2m_client_main, APP_THREAD_PRI, NULL) < 0) {
						eprintf("create thread\n");
					}
				}
			}
		}		
		else{
			//if((loc_ip_ret_eth0 == 1) && (g_ip_route_id != 1)){
			if(g_ip_route_id != 1){	
				eth0_system_route_delete();
				if(id == 1){
					modem_system_route_add(gw_address,device_name);
					g_route_chg_flag = 1;
					if(g_vpn_config_data.vpn_use_flag == CK_USE){
						int loc_pid = get_pid_by_process_name("sgvpn_client");
						if(loc_pid > 0){
							char str_loc_pid[100]={0,};
							sprintf(str_loc_pid,"kill -9 %d",loc_pid);
							system(str_loc_pid);
							tun_set_flag = CK_NO_USE;
						}
					}
				}
				g_route_chg_flag = 1;
				kprintf("Change Route table g_ip_route_id eth0 -> %s\n",device_name);
				st_enti_dual_gw_check_flag = 0;
	
				if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M == 1)){
					if(g_ip_route_id == 0){
						if(g_mtm_server_cockfd != -1)
							close(g_mtm_server_cockfd);	
						thread_delete(_tobjM2mIpSndThread);
						if(thread_create(_tobjM2mIpSndThread, (void *) &socket_m2m_client_main, APP_THREAD_PRI, NULL) < 0) {
							eprintf("create thread\n");
						}
					}
				}
			}
			g_ip_route_id = 1;
		}
	}
	else{
		if(id == 0) kprintf("My system %s IP Address is not setting\n",device_name);	
		if(g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT){
			if((CFUN_flag < 5) && (CFUN_flag == 0)) system(" udhcpc -i eth1");
			if((loc_ip_ret_eth0 == 1) && (loc_link_up_down_ret == 1)){				
				if(id == 0){		
					modem_system_route_delete(gw_address,device_name);
					eth0_system_route_add();
					g_ip_route_id = 0;
					g_route_chg_flag = 1;
					kprintf("Setting Route table g_ip_route_id eth0\n");
				}
				else if(g_ip_route_id == 1){
					if(id == 1){
						eth0_system_route_add();
						if(g_vpn_config_data.vpn_use_flag == CK_USE){
							int loc_pid = get_pid_by_process_name("sgvpn_client");
							if(loc_pid > 0){
								char str_loc_pid[100]={0,};
								sprintf(str_loc_pid,"kill -9 %d",loc_pid);
								system(str_loc_pid);
								tun_set_flag = CK_NO_USE;
							}
						}
					}
					g_route_chg_flag = 1;
					g_ip_route_id = 0;
					kprintf("Change Route table g_ip_route_id %s -> eth0\n",device_name);
		
					if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M == 1)){
						if(g_mtm_server_cockfd != -1)
							close(g_mtm_server_cockfd);
						thread_delete(_tobjM2mIpSndThread);
						if(thread_create(_tobjM2mIpSndThread, (void *) &socket_m2m_client_main, APP_THREAD_PRI, NULL) < 0) {
							eprintf("create thread\n");
						}
					}
				}
				else if(g_ip_route_id == -1){
					eth0_system_route_add();
					g_ip_route_id = 0;
					g_route_chg_flag = 1;
					kprintf("Setting Route table g_ip_route_id eth0\n");
				}
			}
			else{
				if((loc_ip_ret_eth0 == 1) && (g_ip_route_id != 1)){
					eth0_system_route_delete();
					if(g_vpn_config_data.vpn_use_flag == CK_USE){
						int loc_pid = get_pid_by_process_name("sgvpn_client");
						if(loc_pid > 0){
							char str_loc_pid[100]={0,};
							sprintf(str_loc_pid,"kill -9 %d",loc_pid);
							system(str_loc_pid);
							tun_set_flag = CK_NO_USE;
						}
					}
					g_route_chg_flag = 1;
					g_ip_route_id = 1;
					kprintf("Change Route table g_ip_route_id eth0 -> %s\n",device_name);
				}				
			}
		}
		else if(g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT){
			if((loc_ip_ret_eth0 == 1) && (loc_link_up_down_ret == 1)){				
				if(id == 0){	
					eth0_system_route_add();
					g_ip_route_id = 0;
					g_route_chg_flag = 1;
					kprintf("Setting Route table g_ip_route_id eth0\n");
				}
				else if(g_ip_route_id == 1){
					if(id == 1){
						eth0_system_route_add();
						if(g_vpn_config_data.vpn_use_flag == CK_USE){
							int loc_pid = get_pid_by_process_name("sgvpn_client");
							if(loc_pid > 0){
								char str_loc_pid[100]={0,};
								sprintf(str_loc_pid,"kill -9 %d",loc_pid);
								system(str_loc_pid);
								tun_set_flag = CK_NO_USE;
							}
						}
					}
					g_route_chg_flag = 1;
					g_ip_route_id = 0;
					kprintf("Change Route table g_ip_route_id %s -> eth0\n",device_name);
		
					if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M == 1)){
						if(g_mtm_server_cockfd != -1)
							close(g_mtm_server_cockfd);
						thread_delete(_tobjM2mIpSndThread);
						if(thread_create(_tobjM2mIpSndThread, (void *) &socket_m2m_client_main, APP_THREAD_PRI, NULL) < 0) {
							eprintf("create thread\n");
						}
					}
				}
				else if(g_ip_route_id == -1){
					eth0_system_route_add();
					g_ip_route_id = 0;
					g_route_chg_flag = 1;
					kprintf("Setting Route table g_ip_route_id eth0\n");
				}
			}		
			else{
				if(id == 1){
					if(CFUN_flag == 0) system("/usr/local/bin/quectel-pppd.sh");
				}
			}
		}
#ifdef DEF_ENTIMORE_IP_USE	
		else if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
			if((loc_ip_ret_eth0 == 1) && (loc_link_up_down_ret == 1)){				
				if(id == 0){		
					eth0_system_route_add();
					g_ip_route_id = 0;
					g_route_chg_flag = 1;
					kprintf("Setting Route table g_ip_route_id eth0\n");
				}
				else if(g_ip_route_id == 1){
					if(id == 1){
						eth0_system_route_add();
						g_route_chg_flag = 1;
						if(g_vpn_config_data.vpn_use_flag == CK_USE){
							int loc_pid = get_pid_by_process_name("sgvpn_client");
							if(loc_pid > 0){
								char str_loc_pid[100]={0,};
								sprintf(str_loc_pid,"kill -9 %d",loc_pid);
								system(str_loc_pid);
								tun_set_flag = CK_NO_USE;
							}
						}
					}
					g_ip_route_id = 0;
					kprintf("Change Route table g_ip_route_id %s -> eth0\n",device_name);
		
					if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M == 1)){
						if(g_mtm_server_cockfd != -1)
							close(g_mtm_server_cockfd);
						thread_delete(_tobjM2mIpSndThread);
						if(thread_create(_tobjM2mIpSndThread, (void *) &socket_m2m_client_main, APP_THREAD_PRI, NULL) < 0) {
							eprintf("create thread\n");
						}
					}
				}
				else if(g_ip_route_id == -1){
					eth0_system_route_add();
					g_ip_route_id = 0;
					g_route_chg_flag = 1;
					kprintf("Setting Route table g_ip_route_id eth0\n");
				}
			}		
			else{
				if(id == 1){
					if(CFUN_flag == 0){
						kprintf("[%s] ..................... g_ip_route_id= %d\n",__FUNCTION__,g_ip_route_id);
						Entimore_data_link_resetup(0);
					}
				}
			}
		}
#endif
	}
	if(id != 0){
		if((g_ip_route_id == 0) && (g_route_chg_flag ==1)){			
			int ret_d = getrouteState();
			if(ret_d == 2){
				modem_system_route_delete(gw_address,device_name);	
			}
			else if(ret_d < 1) kprintf("ip_link_check_route_setting() default gateway not found\n");
			g_route_chg_flag = 0;
		}
		else if((g_ip_route_id == 1) && (g_route_chg_flag ==1)){			
			int ret_d = getrouteState();
			if(ret_d == 2){
				if((st_enti_dual_gw_check_flag < 5) || (g_modem_comp_id != MODEM_TYPE_ENTIMORE_PRODUCT))
				{		
					if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT) st_enti_dual_gw_check_flag++;
					eth0_system_route_delete();
				}
				g_route_chg_flag = 0;
			}
			else if(ret_d < 1) kprintf("ip_link_check_route_setting() default gateway not found\n");			
		}
	}
	if((g_vpn_config_data.vpn_use_flag == CK_USE) && (tun_set_flag == CK_NO_USE) && ((loc_ip_ret_eth0 == CK_USE) || (loc_ip_ret_eth1 == CK_USE))){
		int loc_ip_ret_tun0 = get_system_MyIPAddress(loc_ip_addr,"tun0");
		kprintf("[%s] loc_ip_ret_tun0 = %d\n",__FUNCTION__,loc_ip_ret_tun0);
		if(loc_ip_ret_tun0 == 0){
			system("/usr/local/bin/run_sgvpn.sh");
			kprintf("[%s] /usr/local/bin/run_sgvpn.sh run\n",__FUNCTION__);
			tun_set_flag = CK_USE;
		}
	}
	g_ip_link_dual_check_flag = 0;
}

void modem_system_route_add(char *gw_address,char *device_name)
{
	char loc_gw_address[100]={0,};
	sprintf(loc_gw_address,"route add default gw %s dev %s",gw_address,device_name);
	system(loc_gw_address);
}

void modem_system_route_delete(char *gw_address,char *device_name)
{
	char loc_gw_address[100]={0,};
	sprintf(loc_gw_address,"route del default gw %s dev %s",gw_address,device_name);
	system(loc_gw_address);
}

void eth0_system_route_add()
{
	char loc_gw_address[100]={0,};
	if(strcmp(g_gateway_ip,"0.0.0.0") == 0){
		unsigned short *loc_ip_addr;
		loc_ip_addr = get_g_loc_lo_ip();
		sprintf(g_gateway_ip,"%d.%d.%d.1",loc_ip_addr[0],loc_ip_addr[1],loc_ip_addr[2]);
	}
	sprintf(loc_gw_address,"route add default gw %s dev eth0",g_gateway_ip);
	system(loc_gw_address);
}

void eth0_system_route_delete()
{
	char loc_route_add_str[100];
	if(strcmp(g_gateway_ip,"0.0.0.0") == 0){
		unsigned short *loc_ip_addr;
		loc_ip_addr = get_g_loc_lo_ip();
		sprintf(g_gateway_ip,"%d.%d.%d.1",loc_ip_addr[0],loc_ip_addr[1],loc_ip_addr[2]);
	}
	sprintf(loc_route_add_str,"route del default gw %s dev eth0",g_gateway_ip);
	system(loc_route_add_str);
}


#ifdef DEF_ENTIMORE_IP_USE
void Entimore_data_link_resetup(int idx)
{
	static int route_flag = 0; 
	
	if(access("/usr/local/bin/ppp_off.sh", F_OK) == 0){
		if(idx == 0){
			if(route_flag == 0){
				if(g_ip_route_id != -1){
					system("/usr/local/bin/ppp_off.sh");
					g_ip_route_id = -1;
				 	g_ppp_run_count = 0;
				}			
			}
		}
		else{
			system("/usr/local/bin/ppp_off.sh");
			if(g_ip_route_id == 1) g_ip_route_id = -1;
		 	g_ppp_run_count = 0;
			route_flag = 1;
			setPPP();			
			sleep(5);
			route_flag = 0;
		}
	}		
	else if(g_ip_route_id != 1){
		g_ppp_run_count = 0;
		setPPP();
		sleep(5);
	}
}
#endif

int getrouteState()
{	
	char buffer[1000];
	FILE *fp;
	
	int ret = 0;
	
	fp = popen("route", "r");
	if(NULL == fp){
		kprintf("[%s] %s\n", __FUNCTION__, "popen() fail");
		return -1;
	}

	while(fgets( buffer, 1000, fp)){
		//kprintf("[%s] %s\n", __FUNCTION__, buffer);
		
		if(strstr(buffer, "default") != NULL){	
			ret++;
		}
	}

	pclose(fp); 
   
    return ret;
}

void get_alarm_n_warning_Read_from_DB()
{
#ifdef DEF_ENTIMORE_IP_USE		
	if((g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT) && (g_ip_route_id == 1)) sleep(7);
	else sleep(3);
#else
	sleep(3);
#endif	
	while(g_mtm_server_cockfd == -1){
		sleep(5);
		continue;
	}
	if(gRegisters.Power_Info.Power_Info_Con.bit.Power_CON2_24VBAT == 1){  //24V 공급에서만 밧데리 정보를 서버로 전송 
		g_battery_alarm = get_system_status_info_read_from_server(1);
		kprintf("get_alarm_n_warning_Read_from_DB() g_battery_alarm= %d\n",g_battery_alarm);
		g_battery_warning = get_system_status_info_read_from_server(2);
		kprintf("get_alarm_n_warning_Read_from_DB() g_battery_warning= %d\n",g_battery_warning);
	}
	else{ 
		g_AC_power_alarm = get_system_status_info_read_from_server(4);
		kprintf("get_alarm_n_warning_Read_from_DB() g_AC_power_alarm= %d\n",g_AC_power_alarm);
	}
	g_modem_svc_alarm = get_system_status_info_read_from_server(3);
	kprintf("get_alarm_n_warning_Read_from_DB() g_modem_svc_alarm= %d\n",g_modem_svc_alarm);
	if(gRegisters.External_System_info.External_System_info_00.bit.External_AMP == CK_USE){
		g_Ext_AMP_alarm = get_system_status_info_read_from_server(5);
		kprintf("get_alarm_n_warning_Read_from_DB() g_Ext_AMP_alarm= %d\n",g_Ext_AMP_alarm);
	}
	if(gRegisters.External_Amp_Info.External_Amp_Option.bit.Speaker_Sensing == CK_USE){
		g_Speaker_alarm = get_system_status_info_read_from_server(6);
		kprintf("get_alarm_n_warning_Read_from_DB() g_Speaker_alarm= %d\n",g_Speaker_alarm);
	}
	if((gRegisters.Communication_info.Communication_info.bit.Comm_con5_422_Analog == CK_USE) || (gRegisters.Communication_info.Communication_info.bit.Comm_con5_422_Digital == CK_USE)){
		g_DIF_alarm = get_system_status_info_read_from_server(7);
		kprintf("get_alarm_n_warning_Read_from_DB() g_DIF_alarm= %d\n",g_DIF_alarm);
	}
	send_to_server_msg_Boot_start_Message();
	if((CFUN_flag != 0) && (CFUN_flag == -1)) modem_service_alarm_ON_OFF_report(0);
}

void send_to_server_msg_Boot_start_Message()
{
	char temp_log[IP_LOG_SEND_MSG_LEN]={0,};
	sprintf(temp_log,"LBST1,%s,NULL,MX400 SYSTEM RUN",gMyCallNumber);
	udp_message_data_send_proc(temp_log,2);
}

int get_system_status_info_read_from_server(int idx)
{
	struct sockaddr_in serveraddr;
	int server_sockfd;
	int client_len;
	int r_len=0,r_info=0;	
	struct    timeval tv; 
    fd_set    readfds;
	char buf[32];
	
	if(strcmp(get_server_ip(),"0.0.0.0") == 0) 
	{
		kprintf("server IP not setting yet\n");
		return 0;
	}
	if(strcmp(gMyCallNumber,"") == 0)
	{
		kprintf("My Call Number is not setting yet!!!\n");
		read_IP_ID_value_to_gMyCallNumber_no_check();
		return 0;
	}
	
	if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)	
	{
		kprintf("socket error: *s\n",strerror(errno));
		return 0;
	}
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(get_server_ip());
	serveraddr.sin_port = htons(13500);
	
	client_len = sizeof(serveraddr);
	
	if(connect(server_sockfd, (struct sockaddr *)&serveraddr, client_len) == -1)
	{
		kprintf("connect error: %s\n",strerror(errno));
		close(server_sockfd);
		return 0;
	}
	
	memset(buf, 0x00, sizeof(buf));	
	struct ip_db_info_request *loc_req = (struct ip_db_info_request *) buf;
	sprintf(loc_req->cmd_id,"DB_INFO_REQ");
	loc_req->info_id = (char)idx;	
	strcpy(loc_req->tel_num,gMyCallNumber);
	
	if(write(server_sockfd, buf, sizeof(buf)) <= 0)
	{
		kprintf("write error: %s\n",strerror(errno));
		close(server_sockfd);
		return 0;
	}			
	memset(buf, 0x00, sizeof(buf));	
	    
    FD_ZERO(&readfds);
    FD_SET(server_sockfd, &readfds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int loc_state = select(server_sockfd + 1, &readfds, (fd_set *)0, (fd_set *)0, &tv);
    switch(loc_state)
    {
        case -1:
            kprintf("[%s] select error : \n",__FUNCTION__);
			close(server_sockfd);
			return 0;
            break;    
        case 0:
            kprintf("[%s] Time over\n",__FUNCTION__);            
            close(server_sockfd);	
			return 0;
            break;
        default:
            if((r_len = read(server_sockfd, buf, sizeof(buf))) <= 0)
			{
				kprintf("[%s] read error : \n",__FUNCTION__);
				close(server_sockfd);
				
			}
			else{
				struct ip_db_info_ack *loc_ack;
				loc_ack = (struct ip_db_info_ack *)buf;
				r_info = loc_ack->result;
				close(server_sockfd);
			}
            break;
    }
	return r_info;
}

int vpn_config_file_checking_n_write()
{
	char buff[300];
	int ret_val = 1,loc_chg_flag = 0;
	
	if(access("/usr/local/sgvpn/conf/sgvpn.conf", F_OK) == -1){
		kprintf("[%s] /usr/local/sgvpn/sgvpn.conf file not found\n",__FUNCTION__);
		return -1;
	}
	FILE *fp = fopen("/usr/local/sgvpn/conf/sgvpn.conf", "rb");
	memset(buff,0,sizeof(buff));
	while(fgets( buff, sizeof(buff), fp)){
		printf("[%s] %s\n", __FUNCTION__, buff);
		if(strstr(buff,"vpn_ip") != NULL){
			if(strstr(buff,g_vpn_config_data.vpn_server_ip) == NULL){
				loc_chg_flag = 1;
			}
		}
		else if(strstr(buff,"vpn_port") != NULL){
			char loc_vpn_port_str[10]={0,};
			sprintf(loc_vpn_port_str,"%d",g_vpn_config_data.vpn_port);
			if(strstr(buff,loc_vpn_port_str) == NULL){
				loc_chg_flag = 1;
			}
		}
		else if(strstr(buff,"userid") != NULL){
			if(strstr(buff,g_vpn_config_data.vpn_user_id) == NULL){
				loc_chg_flag = 1;
			}
		}
		else if(strstr(buff,"userpw") != NULL){
			if(strstr(buff,g_vpn_config_data.vpn_passwd) == NULL){
				loc_chg_flag = 1;
			}
		}
		else if(strstr(buff,"model_name") != NULL){
			if(strstr(buff,g_vpn_config_data.vpn_model_name) == NULL){
				loc_chg_flag = 1;
			}
		}
		else if(strstr(buff,"debug_mode") != NULL){
			if(strstr(buff,g_vpn_config_data.vpn_debug_mode) == NULL){
				loc_chg_flag = 1;
			}
		}
		memset(buff,0,sizeof(buff));
	}
	fclose(fp);
	
	if(loc_chg_flag == 1)
	{
		char loc_vpn_str[1024]={0,},loc_temp_str[100]={0,};
		
		sprintf(loc_vpn_str,"vpn_ip  %s\n",g_vpn_config_data.vpn_server_ip);
		sprintf(loc_temp_str,"vpn_port  %d\n",g_vpn_config_data.vpn_port);
		strcat(loc_vpn_str,loc_temp_str);
		sprintf(loc_temp_str,"userid  %s\n",g_vpn_config_data.vpn_user_id);
		strcat(loc_vpn_str,loc_temp_str);
		sprintf(loc_temp_str,"userpw  %s\n",g_vpn_config_data.vpn_passwd);
		strcat(loc_vpn_str,loc_temp_str);
		sprintf(loc_temp_str,"model_name  %s\n",g_vpn_config_data.vpn_model_name);
		strcat(loc_vpn_str,loc_temp_str);
		sprintf(loc_temp_str,"debug_mode  %s\n",g_vpn_config_data.vpn_debug_mode);
		strcat(loc_vpn_str,loc_temp_str);
		//printf("----------------------------  loc_vpn_str=\n %s\n",loc_vpn_str);
		
		FILE *fp = fopen("/usr/local/sgvpn/conf/sgvpn.conf", "w");
		fwrite(loc_vpn_str, 1, strlen(loc_vpn_str), fp);
  		fclose(fp);
		system("sync");
	
	}
	
	return ret_val;
}

int get_pid_by_process_name(char *process_name)
{
    int pid = -1; 
    char cmd_string[512];
    FILE *fp;

    sprintf(cmd_string, "ps | grep -w %s | grep -v grep", process_name);

    fp = popen(cmd_string, "r");
    fseek(fp, 0, SEEK_SET);
    fscanf(fp, "%d", &pid);

    fclose(fp);

    if (pid < 1)
        pid = -1;

    return pid;
}

unsigned short server_oae_crypt()
{
	char l_char = 's';
	struct timeval loc_time_t;
	struct tm loc_time_tm;
	gettimeofday(&loc_time_t, NULL);
	localtime_r(&loc_time_t.tv_sec, &loc_time_tm);

	char key_date[20];
	sprintf(key_date,"%4d%02d%02d",loc_time_tm.tm_year+1900,loc_time_tm.tm_mon+1,loc_time_tm.tm_mday);
	kprintf("key_date = %s\n",key_date);
  	unsigned short l_mode = oae_crypt(key_date,"12345678",l_char);
  	kprintf("mode = %d\n",l_mode);
	return (l_mode %1000);
}

unsigned short client_oae_crypt()
{
	char l_char = 't';
	struct timeval loc_time_t;
	struct tm loc_time_tm;
	gettimeofday(&loc_time_t, NULL);
	localtime_r(&loc_time_t.tv_sec, &loc_time_tm);

	char key_date[20];
	sprintf(key_date,"%4d%02d%02d",loc_time_tm.tm_year+1900,loc_time_tm.tm_mon+1,loc_time_tm.tm_mday);
	kprintf("client key_date = %s\n",key_date);
	unsigned short l_mode = oae_crypt(key_date,"12345678",l_char);
  	kprintf("mode = %d\n",l_mode);
	return (l_mode %1000);
}

void Chime_Time_setting()
{
#ifndef ADMIN_CHIME_TIME_DEF
#ifdef HAENAM_CHAIM
	if(access("/mmc/sound/MENT/a303.wav", F_OK) >= 0){
		HaeNAM_ChimeBell_PlayTime_OAEPRO = WavReader("/mmc/sound/MENT/a303.wav");
		if(HaeNAM_ChimeBell_PlayTime_OAEPRO < 4500) HaeNAM_ChimeBell_PlayTime_OAEPRO = 4500;
		kprintf("HaeNAM_ChimeBell_PlayTime_OAEPRO = %d (msec)\n",HaeNAM_ChimeBell_PlayTime_OAEPRO);
	}
	else kprintf("/mmc/sound/MENT/a303.wav file not found\n");
#ifdef HAENAM_LEEJANG_CHAIM
	if(access("/mmc/sound/MENT/l062.wav", F_OK) >= 0){
		HaeNAM_ChimeBell_PlayTime_LEEJANG = WavReader("/mmc/sound/MENT/l062.wav");
		kprintf("HaeNAM_ChimeBell_PlayTime_LEEJANG = %d (msec)\n",HaeNAM_ChimeBell_PlayTime_LEEJANG);
	}
	else kprintf("/mmc/sound/MENT/l062.wav file not found\n");
#endif
#ifdef HAENAM_JAENAN_CHAIM
	if(access("/mmc/sound/MENT/j062.wav", F_OK) >= 0){
		HaeNAM_ChimeBell_PlayTime_JAENAN = WavReader("/mmc/sound/MENT/j062.wav");
		kprintf("HaeNAM_ChimeBell_PlayTime_JAENAN = %d (msec)\n",HaeNAM_ChimeBell_PlayTime_JAENAN);
	}
	else kprintf("/mmc/sound/MENT/j062.wav file not found\n");	
#endif	
#else
#ifndef USE_SUJAWON_DEF
	if(access("/mmc/sound/MENT/a062.wav", F_OK) >= 0){
		ChimeBell_PlayTime_OAEPRO = WavReader("/mmc/sound/MENT/a062.wav");
		if(ChimeBell_PlayTime_OAEPRO < 4500) ChimeBell_PlayTime_OAEPRO = 4500;
		kprintf("ChimeBell_PlayTime_OAEPRO = %d (msec)\n",ChimeBell_PlayTime_OAEPRO);
	}
	else kprintf("/mmc/sound/MENT/a062.wav file not found\n");
#endif	
#endif
#endif
}

void water_level_check_proc()
{
	unsigned short loc_water_level_data[WATER_WINDOW_CNT],avg_water_level=0,tot_water_level=0;
	unsigned char loc__water_mod_count=0,loc_check_flag=0;
	char w_buf[30],ack_buf[8],r_val[5];
	app_thr_obj _loc_objWaterLevel_Ment_playThread;
	app_thr_obj *_tloc_objWaterLevel_Ment_playThread = &_loc_objWaterLevel_Ment_playThread; 
	
	int fd_Water = open(SERIAL_DEVNAME, O_RDWR|O_NOCTTY);
	if(fd_Water < 0){
		kprintf("[%s] Device = %s Open Fail!\n",__FUNCTION__,SERIAL_DEVNAME);
		return;
	}
	kprintf("[%s] Device = %s Open OK!!\n",__FUNCTION__,SERIAL_DEVNAME);
	struct termios newtio;

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = S19200_BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 1;

	tcflush(fd_Water, TCIOFLUSH);
	tcsetattr(fd_Water, TCSANOW, &newtio);

	while(1){
		int loc_tmp_len = 0;
		memset(w_buf,0,sizeof(w_buf));
		memset(r_val,0,sizeof(r_val));
		do{
			int re_n = read(fd_Water, &w_buf[loc_tmp_len],sizeof(w_buf));
			if(re_n <= 0)
				break;
			loc_tmp_len = loc_tmp_len + re_n;
		}while(loc_tmp_len < 26);
		if(loc_tmp_len < 26)
			continue;
		kprintf("Read Water Level Data: Len= %d:\n",loc_tmp_len);
		int i;
		for(i=0;i<loc_tmp_len;i++)
			printf("%02X ",w_buf[i]);
		printf("\n");	
		
		//**************** ACK message send ****************************//
		memset(ack_buf,0,sizeof(ack_buf));
		ack_buf[0]= 0x02;
		sprintf(&ack_buf[1],"ACK");
		for(i=1;i<4;i++)
			ack_buf[4] = ack_buf[4] + ack_buf[i];
		//printf("ack_buf CRC= %02X\n",ack_buf[4]);
		ack_buf[5] = 0x0D;
		ack_buf[6] = 0x0A;
		write(fd_Water,ack_buf,7);
		kprintf("[%s] ACK send message\n",__FUNCTION__);
		
		for(i=0;i<4;i++) r_val[i] = w_buf[i+19];
#if 1		
		if(loc_check_flag == 1){
			unsigned short tmp_val = loc_water_level_data[loc__water_mod_count];
			tot_water_level = tot_water_level - tmp_val;
		}
		loc_water_level_data[loc__water_mod_count] = (unsigned short)atoi(r_val);
		kprintf("[%s] receive_water_value= %d\n",__FUNCTION__,loc_water_level_data[loc__water_mod_count]);
		if(atoi(r_val) == 0){
			loc_check_flag = 0;
			loc__water_mod_count = 0;
			tot_water_level= 0;
			memset(loc_water_level_data,0,sizeof(loc_water_level_data));
			thread_delete(_tloc_objWaterLevel_Ment_playThread);
			water_level_data.current_state = 0;
			continue;
		}
		tot_water_level = tot_water_level + loc_water_level_data[loc__water_mod_count];
		loc__water_mod_count++;
		if((loc_check_flag == 0) && (loc__water_mod_count == WATER_WINDOW_CNT)) loc_check_flag = 1;
		
		if(loc_check_flag == 1){
			avg_water_level = (unsigned short)(tot_water_level/WATER_WINDOW_CNT);
			kprintf("[%s] avg_water_level= %d\n",__FUNCTION__,avg_water_level);
			if(water_level_data.ment_play_flag == 1){
				if(water_level_data.critical_level <= avg_water_level){
					if(water_level_data.current_state != 2){
						//쓰레드 죽이고 새로운 쓰레드 생성 
						thread_delete(_tloc_objWaterLevel_Ment_playThread);
						//sprintf(water_level_data.ment_filename,"/mmc/water_ment/critical_ment.wav");
						sprintf(water_level_data.ment_filename,"/mmc/water_ment/critical_ment.wav");
						water_level_data.current_state = 2;
						water_ment_processing(_tloc_objWaterLevel_Ment_playThread);
					}
				}
				else if(water_level_data.major_level <= avg_water_level){
					if(water_level_data.current_state != 1){
						thread_delete(_tloc_objWaterLevel_Ment_playThread); 
						//sprintf(water_level_data.ment_filename,"/mmc/water_ment/major_ment.wav");
						sprintf(water_level_data.ment_filename,"/mmc/water_ment/major_ment.wav");
						water_level_data.current_state = 1;
						water_ment_processing(_tloc_objWaterLevel_Ment_playThread);
					}
				}
				else if(water_level_data.major_level > avg_water_level){
					if(water_level_data.current_state != 0){
						thread_delete(_tloc_objWaterLevel_Ment_playThread);
						water_level_data.current_state = 0;
					}
				}
			}
		}
		loc__water_mod_count = loc__water_mod_count % WATER_WINDOW_CNT;
#endif		
		
	}
	close(fd_Water);
}

void water_ment_processing(app_thr_obj *_tloc_objWaterLevel_Ment_playThread)
{
	if(water_level_data.ment_play_flag == 1){
		if(thread_create(_tloc_objWaterLevel_Ment_playThread, (void *) &water_level_ment_play_processing, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
		}
	}
}

void water_level_ment_play_processing(void *ptr)
{
	while(1){
		sendBroadcastMsgFilePlay(MSG_FILE_PLAY_BOARDCAST_START, water_level_data.ment_filename, BROADCAST_GROUP_ALL, CK_USE, CK_USE);	
		if(water_level_data.ment_play_period != 0) sleep(water_level_data.ment_play_period * 60);
		else return;
	}
}

void Modem_Initial_Processing()
{
	if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
		if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) modem_cdma_init(1);
		else modem_cdma_init(0);
	}	
	else if(g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT){	
		int loop_i =0,ii;
		if(g_mdm_rst_count == 0) loop_i = 1;
		else loop_i = 2;
		for(ii=0;ii < loop_i;ii++){
			Telradin_modem_init();
			if(CFUN_flag == 0) break;
			if(g_mdm_rst_count == 1) sleep(10);
		}
	}	
	else if(g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT){
		mtom_modem_init();
	}

	
}

void Modem_ip_link_setup()
{
	if(g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT){
		if(CFUN_flag == 0)
			ip_link_check_route_setting(0,DEF_TELADIN_GW,DEF_TELADIN_IP_DEVICE);
	}	
	else if(g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT){
		if(CFUN_flag == 0){
			char loc_ip_addr[50];
			int loc_ip_ret_ppp0 = get_system_MyIPAddress(loc_ip_addr,DEF_MTOM_IP_DEVICE);
			kprintf("loc_ip_ret_ppp0 = %d\n",loc_ip_ret_ppp0);
			if(loc_ip_ret_ppp0 == 0)
				system("/usr/local/bin/quectel-pppd.sh");
			ip_link_check_route_setting(0,DEF_MTOM_GW,DEF_MTOM_IP_DEVICE);
		}
	}
#ifdef DEF_ENTIMORE_IP_USE		
	else if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
		if(CFUN_flag == 0){
			char loc_ip_addr[50];
			int loc_ip_ret_ppp0 = get_system_MyIPAddress(loc_ip_addr,DEF_ENTIMORE_IP_DEVICE);
			kprintf("loc_ip_ret_ppp0 = %d\n",loc_ip_ret_ppp0);
			if(loc_ip_ret_ppp0 == 0){
				setPPP();
				sleep(3);
			}
			ip_link_check_route_setting(0,DEF_ENTIMORE_GW,DEF_ENTIMORE_IP_DEVICE);
		}
	}
#endif	
}

void IP_Broadcast_Thread_Generation(app_thr_obj *tObj)
{
#ifndef USE_SUJAWON_DEF
	//init_encode_decode();

	if((gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1) || (gRegisters.Communication_info.Communication_info.bit.Comm_con12_IP_M2M == 1)){
		kprintf("Entimore Modem IP Broadcation Thread Run!!!\n");					
		
		if(thread_create(_tobjM2mIpSndThread, (void *) &socket_m2m_client_main, APP_THREAD_PRI, NULL) < 0) {
				eprintf("create thread\n");
		}
		if(thread_create(tObj, (void *)&IpStsTfunction, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
		}
		if(thread_create(tObj, (void *)&get_alarm_n_warning_Read_from_DB, APP_THREAD_PRI, NULL) < 0) {
			eprintf("create thread\n");
		} 
	}
#else
	if(gRegisters.Communication_info.Communication_info.bit.Comm_con7_Ethernet == 1){
		if(thread_create(tObj, (void *) &Sujawon_server_main, APP_THREAD_PRI, NULL) < 0) {
				eprintf("create thread\n");
		}
	}
#endif
}

#ifdef DEF_DOOR_OPEN_SEND_MSG_USE   //door open SMS report processing
void door_open_check_proc()
{
	int door_open_status = 1;  //door closed
	
	sleep(10);
	kprintf("[%s] Door Open CHeck Thread Start!!!!,door_check_data.di_num= %d\n",__FUNCTION__,door_check_data.di_num);

	while(1){
		if(door_check_data.used_flag == CK_USE){
			int ret_v = readPowerBoard_DI_for_door_open(door_check_data.di_num,door_check_data.open_value);
			//printf("[%s] readPowerBoard_DI_for_door_open ret_v= %d\n",__FUNCTION__,ret_v);
			if(ret_v != -1){
				if(door_open_status != ret_v){
					door_open_status = ret_v;
					if((door_check_data.rcv_tel_num[0] != 0) && (door_check_data.report_flag == CK_USE)){
						kprintf("[%s] send_SMS_message_for_door_open_close= %d\n",__FUNCTION__,door_open_status);
						send_SMS_message_for_door_open_close(door_open_status);
					}
					else if(door_check_data.rcv_tel_num[0] == 0){
						kprintf("[%s] Reported Telephone number is not setting!!!!\n",__FUNCTION__);
					}
#ifdef DEF_DOOR_OPEN_BROADCAST_USE					
					if((door_check_data.rcv_tel_num[0] != 0) && (door_check_data.ment_number != 0) && (door_check_data.broad_count != 0)){
						if(door_open_status == 1){
							aplay_play_stop();						
						}
						else{
							 //방송 멘트 play
							
						}
					}
#endif					
				}
			}
		}
		sleep(1);
	}

}


void send_SMS_message_for_door_open_close(int on_off_flag)
{
	char hex_send_data[180], send_data[90];
#if 1
	//sprintf(send_data,"마을 이름 SMS 도어(0-열림, 1-닫힘)= %d",on_off_flag);
	if(gRegisters.Equipment_Vill_name.bill_Name[0] == 0) sprintf(send_data,"%s 도어(0-열림, 1-닫힘)= %d",gMyCallNumber,on_off_flag);
	else sprintf(send_data,"%s 도어(0-열림, 1-닫힘)= %d",gRegisters.Equipment_Vill_name.bill_Name,on_off_flag);
#else
	if(door_check_data.sms_message[0] != 0){
		if(gRegisters.Equipment_Vill_name.bill_Name[0] == 0) sprintf(send_data,"%s %s= %d",gMyCallNumber,door_check_data.sms_message,on_off_flag);
		else sprintf(send_data,"%s %s= %d",gRegisters.Equipment_Vill_name.bill_Name,door_check_data.sms_message,on_off_flag);
	}
	else{
		if(gRegisters.Equipment_Vill_name.bill_Name[0] == 0) sprintf(send_data,"%s 도어(0-열림, 1-닫힘)= %d",gMyCallNumber,on_off_flag);
		else sprintf(send_data,"%s 도어(0-열림, 1-닫힘)= %d",gRegisters.Equipment_Vill_name.bill_Name,on_off_flag);
	}
#endif	
	if(g_modem_comp_id == MODEM_TYPE_ENTIMORE_PRODUCT){
		int i;
		char loc_temp[3];
		memset(hex_send_data,0,180);
		for(i=0;i<strlen(send_data);i++){
			sprintf(loc_temp,"%02X",send_data[i]);
			strcat(hex_send_data,loc_temp);
		}
		send_SMS_Message(hex_send_data,0,SMS_DOOR_ID);
	}
	else if((g_modem_comp_id == MODEM_TYPE_TELADIN_PRODUCT) || (g_modem_comp_id == MODEM_TYPE_MTOM_PRODUCT)) send_SMS_Message(send_data,0,SMS_DOOR_ID);
}

#endif

#ifdef DEF_RESERVED_BROADCAST_USE
void reserved_file_directory_check_n_creat()
{
	if(access("/mmc/reserved_file", 0) == -1){
		system("mkdir /mmc/reserved_file");
	}
	if(access("/mmc/reserved_list", 0) == -1){
		system("mkdir /mmc/reserved_list");
	}
	if(access("/mmc/reserved_list/reserved_list", F_OK) < 0){
		memset(&g_rsv_read_data,0,sizeof(Rsv_Setting_Data));
		kprintf("[%s] /mmc/reserved_list/reserved_list file not exist!!\n",__FUNCTION__);
		return;
	}
	Read_from_reserved_list_file();
}

void Read_from_reserved_list_file()
{
	FILE *pFile = NULL; 
	int index_id = 0;
	pFile = fopen( "/mmc/reserved_list/reserved_list", "r" );
	if( pFile != NULL )
	{
		char strTemp[255];
		char *pStr;

		memset(&g_rsv_read_data,0,sizeof(Rsv_Setting_Data));
		
		while(!feof(pFile))
		{
			memset(strTemp,0,sizeof(strTemp));
			pStr = fgets( strTemp, sizeof(strTemp), pFile );
			char *ptr = strtok(strTemp, ":");  
			int jj=0,chk_flag=0;
			while (ptr != NULL)               
			{
				//printf("ptr: %s\n", ptr); 
				if(jj == 0)	g_rsv_read_data.rsv_cfg_data[index_id].date_hour[0] =  atoi(ptr);
				else if(jj == 1) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[1] =  atoi(ptr);
				else if(jj == 2) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[2] =  atoi(ptr);
				else if(jj == 3) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[3] =  atoi(ptr);
				else if(jj == 4) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[4] =  atoi(ptr);
				else if(jj == 5) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[5] =  atoi(ptr);
				else if(jj == 6) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[6] =  atoi(ptr);
				else if(jj == 7) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[7] =  atoi(ptr);
				else if(jj == 8) g_rsv_read_data.rsv_cfg_data[index_id].date_hour[8] =  atoi(ptr);
				else if(jj == 9) g_rsv_read_data.rsv_cfg_data[index_id].w_day = atoi(ptr);
				else if(jj == 10){
					sprintf(g_rsv_read_data.rsv_cfg_data[index_id].file_name,"%s",ptr);
					chk_flag =1;
					g_rsv_read_data.rsv_cfg_data[index_id].used_flag = 1;
					g_rsv_read_data.rsv_cfg_data[index_id].next_id = 255;
				}
				ptr = strtok(NULL, ":");  
				jj++;
			}
			if(chk_flag == 1) {
				if(index_id > 0) g_rsv_read_data.rsv_cfg_data[index_id-1].next_id = index_id;
				index_id++;
			}
		}
		g_rsv_read_data.rsv_count = index_id;
		g_rsv_read_data.start_id = 0;
		fclose(pFile);
	}	
}

void reserved_broadcast_processing()
{
	struct timeval time_t;
	struct tm *time_tm;
	int loc_sec,loc_check_flag=0;
	kprintf("[%s] Reserved broadcast Tread Run!!!!\n",__FUNCTION__);
	while(1){
		gettimeofday(&time_t, NULL);
		time_tm = localtime(&time_t.tv_sec);
		kprintf("[%s] g_rsv_read_data.rsv_count = %d\n",__FUNCTION__,g_rsv_read_data.rsv_count);
		if(g_rsv_read_data.rsv_count > 0){
			int i=0,loc_phour,loc_pmin,loc_pyear,loc_pmon,loc_pday,loc_pwday;
			loc_pyear = time_tm->tm_year+1900;
			loc_pmon = time_tm->tm_mon+1;
			loc_pday = time_tm->tm_mday;
			loc_phour = time_tm->tm_hour;
			loc_pmin = time_tm->tm_min;
			loc_pwday = time_tm->tm_wday;
			printf("loc_pyear=%d, loc_pmon= %d,loc_pday= %d,loc_phour= %d,loc_pmin = %d,loc_pwday= %d\n",\
					loc_pyear,loc_pmon,loc_pday,loc_phour,loc_pmin,loc_pwday);
			i = g_rsv_read_data.start_id;
			if(i != 255){
				do{
					unsigned short loc_hour,loc_min,loc_syear,loc_smon,loc_sday,loc_eyear,loc_emon,loc_eday;
					loc_hour = g_rsv_read_data.rsv_cfg_data[i].date_hour[0];
					loc_min = g_rsv_read_data.rsv_cfg_data[i].date_hour[1];
					loc_syear = g_rsv_read_data.rsv_cfg_data[i].date_hour[2];
					loc_smon = g_rsv_read_data.rsv_cfg_data[i].date_hour[3];
					loc_sday = g_rsv_read_data.rsv_cfg_data[i].date_hour[4];
					loc_eyear = g_rsv_read_data.rsv_cfg_data[i].date_hour[5];
					loc_emon = g_rsv_read_data.rsv_cfg_data[i].date_hour[6];
					loc_eday = g_rsv_read_data.rsv_cfg_data[i].date_hour[7];
					
					if((loc_pyear >= loc_syear) && (loc_pyear <= loc_eyear) && (loc_pmon >= loc_smon) && (loc_pmon <= loc_emon) && \
						(loc_pday >= loc_sday) && (loc_pday <= loc_eday)){
						//printf("g_rsv_read_data.rsv_cfg_data[%d].w_day= %d\n",i,g_rsv_read_data.rsv_cfg_data[i].w_day);
						unsigned char loc_wday_flag = (g_rsv_read_data.rsv_cfg_data[i].w_day >> loc_pwday) & 0x01;
						if((loc_wday_flag == 1) && (loc_phour == loc_hour) && (loc_pmin == loc_min)){
							if(getCurrentBroadcastState() == BOARDCAST_STATE_NONE){
								loc_check_flag = 2;
								g_rsv_read_data.current_index = i;
								char temp_file_str[1000]={0,};
								sprintf(temp_file_str,"/mmc/reserved_file/%s",g_rsv_read_data.rsv_cfg_data[i].file_name);
								//printf("reserved broadcasting file = %s\n",temp_file_str);
								send_FileMsg_to_BroadcastThread(MSG_LOC_RSV_FILE_PLAY_BOARDCAST_START, temp_file_str, BROADCAST_GROUP_ALL, CK_USE, CK_USE,BOARDCAST_TYPE_LOC_RSV_FILE_PLAY);
							}
							else{
								loc_check_flag = 1;
								sleep(1);
								continue;
							}
						}
						else loc_check_flag = 0;
					}
					else loc_check_flag = 0;
					
					if(loc_check_flag == 2){  //같은 시간에 2번 등록 안됨 
						loc_check_flag = 0;
						break;
					}
					i = g_rsv_read_data.rsv_cfg_data[i].next_id;
					
					if(i == 255){
						loc_check_flag = 0;
						break;
					}
				}while(g_rsv_read_data.rsv_cfg_data[i].used_flag == 1);
			}
		}
		else loc_check_flag = 0;
		
		gettimeofday(&time_t, NULL);
		time_tm = localtime(&time_t.tv_sec);
		if(loc_check_flag == 0){
			loc_sec = 61 - time_tm->tm_sec;
			if(loc_sec > 0) sleep(loc_sec);
			else sleep(60);		
		}
		else{
			sleep(1);
		}
		loc_check_flag = 0;
	}
}
#endif

#ifdef DEF_OLD_BRD_USE
#ifdef DEF_HW_WATCHDOG_USE
void repeat_Wdt_wdi_On_OFF()
{
	unsigned char loc_loop = 0;
	while(1){
		if(loc_loop == 0){
			setPowerBoardWatchdog(SET_POWER_BOARD_ON_OFF_WDT_WDI, CK_OFF);			
			loc_loop ++;
		}
		else{
			setPowerBoardWatchdog(SET_POWER_BOARD_ON_OFF_WDT_WDI, CK_ON);			
			loc_loop = 0;
		}
		app_thr_wait_msecs(500);
	}
}
#endif
#endif

void kill_watchdog_process()
{
	if(g_hwwatch_pid != -1){
		char loc_cmd[100]={0,};
		sprintf(loc_cmd,"kill -9 %d",g_hwwatch_pid);
		printf("=================>>>>>> kill_watchdog_process() : %s\n",loc_cmd);
		system("sync");
		system(loc_cmd);
	}
}

void HW_watchdog_disable()
{
	kprintf("[%s] H/W Watchdog Disable!!!\n",__FUNCTION__);
#ifdef DEF_OLD_BRD_USE	
	//etPowerBoardOnOff(SET_POWER_BOARD_ON_OFF_WDT_EN, CK_ON);	//watcgdog disable 
	setPowerBoardWatchdog(SET_POWER_BOARD_ON_OFF_WDT_EN, CK_ON);	//watcgdog disable 
	app_thr_wait_msecs(100);
#else
	int val = I2C_Read_Data_from_Device_Register(3);
	if(val != -1){
		val = val & 0xFFFC;
		val = val | 0xFFFE;
		I2C_Write_Data_from_Device_Register(3,val,2);
	}
	app_thr_wait_msecs(500);
#endif	
}

void HW_watchdog_enable()
{
	printf("[%s] H/W Watchdog Enable!!!\n",__FUNCTION__);
#ifdef DEF_OLD_BRD_USE	
	//setPowerBoardOnOff(SET_POWER_BOARD_ON_OFF_WDT_EN, CK_OFF);	//watcgdog disable 
	setPowerBoardWatchdog(SET_POWER_BOARD_ON_OFF_WDT_EN, CK_OFF);	//watcgdog disable 
	app_thr_wait_msecs(100);
#endif	
}

