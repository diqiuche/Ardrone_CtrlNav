
#pragma warning( disable : 4996 ) // disable deprecation warning 

#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#include <process.h>
#include <Windows.h>
#include <VP_Os/vp_os_malloc.h>
#include <VP_Os/vp_os_print.h>
#include <VP_Api/vp_api_thread_helper.h>

#include <VP_Com/vp_com.h>

#include <ardrone_tool/ardrone_tool.h>
#include <ardrone_tool/ardrone_time.h>
#include <ardrone_tool/Control/ardrone_control.h>
#include <ardrone_tool/Control/ardrone_control_ack.h>
#include <ardrone_tool/Navdata/ardrone_navdata_client.h>
#include <ardrone_tool/UI/ardrone_input.h>
#include <ardrone_tool/Com/config_com.h>
#include <custom_code.h>
#ifndef __C_PLUS_PLUS
#define bool int
 #define true 1
 #define false 0
#endif

#include <wbemidl.h>
static int start = 0;
static float roll = 0, pitch = 0, gaz=0, yaw=0;
static int emergency=0;
static float roll_p=0,pitch_p=0,gaz_p=0,yaw_p=0;
static int emergency_p=0;
static int takeoff=0,takeoff_p=0;
static int trim=0,trim_p=0;
static int hover=0,hover_p=0;

int32_t MiscVar[NB_MISC_VARS] = { 
               DEFAULT_MISC1_VALUE, 
               DEFAULT_MISC2_VALUE,
               DEFAULT_MISC3_VALUE, 
               DEFAULT_MISC4_VALUE
                                };

static bool_t need_update   = TRUE;
static ardrone_timer_t ardrone_tool_timer;
static int ArdroneToolRefreshTimeInMs = ARDRONE_REFRESH_MS;
SOCKET ConnectSocket;

unsigned long bswap(unsigned long x) { return _byteswap_ulong(x); }

int clz(unsigned long x)
{
	
	int i; const int L=sizeof(x)*8-1;
	const unsigned long mask = ( 1 << L );
	if (x==0) { return L+1; }
	for (i=0;i<L;i++) { if (x&mask) return i; x<<=1; } 
	return i;
}


char wifi_ardrone_ip[256] = { WIFI_ARDRONE_IP };

/*
 * 为适应ARDrone SDK 1.8 对飞行器配置方式的修改，
	需要在此添加如下9行代码，才能正确的使用<ardrone_tool/ardrone_tool_configuration.h>
	中定义的宏 ARDRONE_TOOL_CONFIGURATION_ADDEVENT，来修改ardrone的配置。
 */
char app_id [MULTICONFIG_ID_SIZE] = "00000000"; // Default application ID.
char app_name [APPLI_NAME_SIZE] = "Default application"; // Default application name.
char usr_id [MULTICONFIG_ID_SIZE] = "00000000"; // Default user ID.
char usr_name [USER_NAME_SIZE] = "Default user"; // Default user name.
char ses_id [MULTICONFIG_ID_SIZE] = "00000000"; // Default session ID.
char ses_name [SESSION_NAME_SIZE] = "Default session"; // Default session name.

#ifndef __SDK_VERSION__
#define __SDK_VERSION__ "1.8" // TEMPORARY LOCATION OF __SDK_VERSION__ !!!
#endif

ardrone_tool_configure_data_t configure_data[] = {
  { "general:navdata_demo", "FALSE" },
  { NULL, NULL }
};



static int32_t configure_index = 0;
static ardrone_control_ack_event_t ack_config;
static bool_t send_com_watchdog = FALSE;


void ardrone_tool_send_com_watchdog( void )
{
  send_com_watchdog = TRUE;
}


static void ardrone_tool_end_configure( struct _ardrone_control_event_t* event )
{
  if( event->status == ARDRONE_CONTROL_EVENT_FINISH_SUCCESS )
    configure_index ++;

  if( configure_data[configure_index].var != NULL && configure_data[configure_index].value != NULL )
  {
    ack_config.event                        = ACK_CONTROL_MODE;
    ack_config.num_retries                  = 20;
    ack_config.status                       = ARDRONE_CONTROL_EVENT_WAITING;
    ack_config.ardrone_control_event_start  = NULL;
    ack_config.ardrone_control_event_end    = ardrone_tool_end_configure;
    ack_config.ack_state                    = ACK_COMMAND_MASK_TRUE;

    //ardrone_at_set_toy_configuration( configure_data[configure_index].var, configure_data[configure_index].value );
	ardrone_at_set_toy_configuration_ids( configure_data[configure_index].var, ses_id, usr_id, app_id,
		configure_data[configure_index].value );
    ardrone_at_send();

    ardrone_control_send_event( (ardrone_control_event_t*)&ack_config );
  }
}

static C_RESULT ardrone_tool_configure()
{
  if( configure_data[configure_index].var != NULL && configure_data[configure_index].value != NULL )
  {
    ack_config.event                        = ACK_CONTROL_MODE;
    ack_config.num_retries                  = 20;
    ack_config.status                       = ARDRONE_CONTROL_EVENT_WAITING;
    ack_config.ardrone_control_event_start  = NULL;
    ack_config.ardrone_control_event_end    = ardrone_tool_end_configure;
    ack_config.ack_state                    = ACK_COMMAND_MASK_TRUE;

//  ardrone_at_set_toy_configuration( configure_data[configure_index].var, configure_data[configure_index].value );
	ardrone_at_set_toy_configuration_ids( configure_data[configure_index].var, ses_id, usr_id, app_id,
		configure_data[configure_index].value );
    ardrone_at_send();

    ardrone_control_send_event( (ardrone_control_event_t*)&ack_config );
  }

  return C_OK;
}

static void ardrone_toy_network_adapter_cb( const char* name )
{
  strcpy( COM_CONFIG_NAVDATA()->itfName, name );
}

C_RESULT ardrone_tool_setup_com( const char* ssid )
{
  C_RESULT res = C_OK;

  vp_com_init(COM_NAVDATA());
  vp_com_local_config(COM_NAVDATA(), COM_CONFIG_NAVDATA());
  vp_com_connect(COM_NAVDATA(), COM_CONNECTION_NAVDATA(), NUM_ATTEMPTS);
  ((vp_com_wifi_connection_t*)wifi_connection())->is_up=1;
  return res;
}

C_RESULT ardrone_tool_init(int argc, char **argv)
{
	C_RESULT res;
	ardrone_at_init( wifi_ardrone_ip, strlen( wifi_ardrone_ip) );
	ardrone_timer_reset(&ardrone_tool_timer);
	ardrone_control_init();
	ardrone_navdata_client_init();
	res = ardrone_tool_init_custom(argc, argv);
	ardrone_at_open();
	START_THREAD(ardrone_control, 0);
	ardrone_tool_configure();


	ardrone_at_set_pmode( MiscVar[0] );
	ardrone_at_set_ui_misc( MiscVar[0], MiscVar[1], MiscVar[2], MiscVar[3] );
	
	return res;
}


C_RESULT ardrone_tool_set_refresh_time(int refresh_time_in_ms)
{
  ArdroneToolRefreshTimeInMs = refresh_time_in_ms;

  return C_OK;
}

C_RESULT ardrone_tool_pause( void )
{
   ardrone_navdata_client_suspend();

   return C_OK;
}

C_RESULT ardrone_tool_resume( void )
{
   ardrone_navdata_client_resume();

   return C_OK;
}

C_RESULT ardrone_control(){
	const char * linefiller="                                              ";
   static int nCountFrequency=0;
	static bool function_first_call = true;
	nCountFrequency++;
	if(nCountFrequency%50==0)
	{
		printf("过去了  %d    秒！！！！\n",nCountFrequency/50);
		if(nCountFrequency%5000==0)
			nCountFrequency=0;
	}
	if (function_first_call) { 
		printf("Sending flat trim - make sure the drone is horizontal at client startup.\n"); 
		ardrone_at_set_flat_trim(); //向无人机发送确定无人机是水平躺着，每次无人机启动都要发送，不可在无人机飞行时调用此函数
		function_first_call=false; 
	//	vp_os_memcpy(previous_keyboardState,keyboardState,sizeof(keyboardState));
		return C_OK; 
		
	}

	ARWin32Demo_AcquireConsole();
	ARWin32Demo_SetConsoleCursor(0,12);

	if(emergency!=emergency_p){  //以下代码 待服务器端完成后，需再次修改
		if(emergency=0)
		{
			ardrone_tool_set_ui_pad_start(0); 
				ardrone_tool_set_ui_pad_select(1);  
				printf("Sending emergency.%s\n",linefiller); 
		}
		else//若之前按下，现在没按，说明是emergency状态转常规飞行
			{
					ardrone_tool_set_ui_pad_select(0);  
			}
	}

	if((takeoff!=takeoff_p)&&takeoff==1)
	{
			start^=1; 
			ardrone_tool_set_ui_pad_start(start);  
			printf("飞机起飞 %i.%s\n",start,linefiller); 
	}
	if((trim!=trim_p)&&trim==1)
	{
		ardrone_at_set_flat_trim(); 
		printf("水平矫正.%s\n",linefiller);
	}

	ardrone_at_set_progress_cmd(hover,roll, pitch, gaz, yaw);
	printf("[Pitch %f] [Roll %f] [Yaw %f] [Gaz %f]%\n",pitch,roll,yaw,gaz);

	ARWin32Demo_ReleaseConsole();

	return C_OK;
}

C_RESULT ardrone_tool_update()
{
	int delta;

	C_RESULT res = C_OK; //C_RESULT 是定义的宏 其对应的是int类型，C_OK也是，其对应的值是0

	if( need_update )
	{
		ardrone_timer_update(&ardrone_tool_timer);//更新时间，为什么要这么处理

		ardrone_control();
		res = ardrone_tool_update_custom();

		if( send_com_watchdog == TRUE )
		{
			ardrone_at_reset_com_watchdog();
			send_com_watchdog = FALSE;
		}//watchdog看门狗防止程序出现死循环，不受控制
		ardrone_at_send();

		need_update = FALSE;
	}

	delta = ardrone_timer_delta_ms(&ardrone_tool_timer);
	if( delta >= ArdroneToolRefreshTimeInMs)
	{
		res = ardrone_tool_display_custom();
		need_update = TRUE;
	}
	else
	{
		Sleep((ArdroneToolRefreshTimeInMs - delta));
	}

	return res;
}

C_RESULT ardrone_tool_shutdown()
{
  C_RESULT res = C_OK;
  
#ifndef NO_ARDRONE_MAINLOOP
  res = ardrone_tool_shutdown_custom();
#endif

  ardrone_navdata_client_shutdown();
  ardrone_control_shutdown();
 
  JOIN_THREAD(ardrone_control); 
  JOIN_THREAD(navdata_update);

  ATcodec_exit_thread();
  ATcodec_Shutdown_Library();

  vp_com_disconnect(COM_NAVDATA());
  vp_com_shutdown(COM_NAVDATA());

  PRINT("Custom ardrone tool ended\n");

  return res;
}


int test_drone_connection()
{
	const char * passivdeModeHeader = "\r\n227 PASV ok (";
	vp_com_socket_t ftp_client,ftp_client2;
	char buffer[1024];
	static Write ftp_write = NULL;
	static Read  ftp_read = NULL;
	int bytes_to_send,received_bytes;
	int i,L,x[6],port;
	int timeout_windows = 1000; /*milliseconds*/
	
	vp_os_memset(buffer,0,sizeof(buffer));

	wifi_config_socket(&ftp_client,VP_COM_CLIENT,FTP_PORT,WIFI_ARDRONE_IP);
	ftp_client.protocol = VP_COM_TCP;
	if(VP_FAILED(vp_com_init(wifi_com()))) return -1;
	if(VP_FAILED(vp_com_open(wifi_com(), &ftp_client, &ftp_read, &ftp_write))) return -2;
	setsockopt((int32_t)ftp_client.priv, 
							SOL_SOCKET, 
							SO_RCVTIMEO, 
							(const char*)&timeout_windows, sizeof(timeout_windows)
							); 

	bytes_to_send = _snprintf(buffer,sizeof(buffer),"%s",
		"USER anonymous\r\nCWD /\r\nPWD\r\nTYPE A\r\nPASV\r\nRETR version.txt\r\n");
	ftp_write(&ftp_client,buffer,&bytes_to_send);
	Sleep(1000);


	received_bytes = sizeof(buffer);
	ftp_read(&ftp_client,buffer,&received_bytes);
	if (received_bytes<1) { vp_com_close(wifi_com(), &ftp_client); return -3; }
	L=received_bytes-strlen(passivdeModeHeader);

	for (i=0;i<L;i++) {
		if (strncmp((buffer+i),passivdeModeHeader,strlen(passivdeModeHeader))==0)  break; 
	}
	if (i==L) {
		vp_com_close(wifi_com(), &ftp_client); return -4; 
	}
	i+=strlen(passivdeModeHeader);
	if (sscanf(buffer+i,"%i,%i,%i,%i,%i,%i)",&x[0],&x[1],&x[2],&x[3],&x[4],&x[5])!=6)
		{ vp_com_close(wifi_com(), &ftp_client); return -5; }
	port=(x[4]<<8)+x[5];

	wifi_config_socket(&ftp_client2,VP_COM_CLIENT,port,"192.168.1.1");
	ftp_client2.protocol = VP_COM_TCP;
	if(VP_FAILED(vp_com_init(wifi_com()))) 
			{ vp_com_close(wifi_com(), &ftp_client2); return -6; }
	if(VP_FAILED(vp_com_open(wifi_com(), &ftp_client2, &ftp_read, &ftp_write)))
		{ vp_com_close(wifi_com(), &ftp_client2); return -7; }

	received_bytes = sizeof(buffer);
	ftp_read(&ftp_client2,buffer,&received_bytes);
	if (received_bytes>0) {
		buffer[min(received_bytes,sizeof(buffer)-1)]=0;
		printf("无人机版本 %s 被检测到 ... 按下 <Enter> 开始应用.\n",buffer);
		getchar();
	}
	

	vp_com_close(wifi_com(), &ftp_client);
	vp_com_close(wifi_com(), &ftp_client2);

return 0;
}
int sdk_demo_stop=0;
void receive_info(void * v){   //需再次修改
	int iResult;
	int recvbuflen=DEFAULT_BUFLEN;
	char recvbuf[1000];
	while(1)
	{
		 iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if ( iResult > 0 )
            printf("Bytes received: %d\n", iResult);
        else if ( iResult == 0 )
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());
	 // sleep(1000);
	}
}
int  __cdecl  init_client(){  //需再次修改
	 WSADATA wsaData;
    //ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL;
	struct addrinfo *ptr = NULL;
	struct addrinfo hints;
  // char *sendbuf = "this is a test";
//    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
	 iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

   // ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, 
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }
		   iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }
	return 0;
}


/*void test(void * null){
	while(1){
		printf("将来在这里处理无人机控制事件");
		//sleep(2);
	}}*/
int main(int argc, char **argv)
{
	  C_RESULT res;				
	  const char* appname = argv[0];
	  WSADATA wsaData = {0};
	  int iResult = 0;
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {	wprintf(L"WSAStartup failed: %d\n", iResult);	return 1;	}
			

		#include <VP_Os/vp_os_signal.h>
			#if defined USE_PTHREAD_FOR_WIN32
			#pragma comment (lib,"pthreadVC2.lib")
			#endif
 
		res = test_drone_connection();//检测WiFi连接
		if(res!=0){
			printf("%s","Could not detect the drone version ... press <Enter> to try connecting anyway.\n");
			getchar();
			WSACleanup(); 
			exit(-1);
		}
	     
		init_client();
		res = ardrone_tool_setup_com( NULL );//配置AT命令，视频传输
		if( FAILED(res) ){  PRINT("Wifi initialization failed.\n");  return -1;	}

	   res = ardrone_tool_init(argc, argv);//调用ardrone_tool_init_custom()函数
      while( VP_SUCCEEDED(res) && ardrone_tool_exit() == FALSE ) {
        res = ardrone_tool_update();     }//循环调用ardrone_too_update()函数

      res = ardrone_tool_shutdown();
     	closesocket(ConnectSocket);
	  WSACleanup();
	system("cls");
	  printf("End of SDK Demo for Windows\n");
	  getchar();
	  return VP_SUCCEEDED(res) ? 0 : -1;
}


// Default implementation for weak functions
	C_RESULT ardrone_tool_update_custom() { return C_OK; }
	C_RESULT ardrone_tool_display_custom() { return C_OK; }
	C_RESULT ardrone_tool_check_argc_custom( int32_t argc) { return C_OK; }
	void ardrone_tool_display_cmd_line_custom( void ) {}
	bool_t ardrone_tool_parse_cmd_line_custom( const char* cmd ) { return TRUE; }


