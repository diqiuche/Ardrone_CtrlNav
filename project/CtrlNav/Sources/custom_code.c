#include <custom_code.h>

//ARDroneLib
#include <ardrone_tool/ardrone_time.h>
#include <ardrone_tool/Navdata/ardrone_navdata_client.h>
#include <ardrone_tool/Control/ardrone_control.h>
#include <ardrone_tool/UI/ardrone_input.h>

//Common
#include <config.h>
#include <ardrone_api.h>

//VP_SDK
#include <ATcodec/ATcodec_api.h>
#include <VP_Os/vp_os_print.h>
#include <VP_Api/vp_api_thread_helper.h>
#include <VP_Os/vp_os_signal.h>

//Global variables
int32_t exit_ihm_program = 1;
vp_os_mutex_t consoleMutex;

C_RESULT ardrone_tool_init_custom(int argc, char **argv)
{

    vp_os_mutex_init(&consoleMutex);
    system("cls");
    SetConsoleTitle(TEXT("Parrot A.R. Drone SDK Demo for Windows"));
   
    START_THREAD(navdata_update, NULL);

    return C_OK;
}


C_RESULT ardrone_tool_shutdown_custom()
{
 
    return C_OK;
}


bool_t ardrone_tool_exit()
{
    return exit_ihm_program == 0;
}

C_RESULT signal_exit()
{
    exit_ihm_program = 0;

    return C_OK;
}

int custom_main(int argc,char**argv)
{
    return 0;
};


BEGIN_THREAD_TABLE   //thread_table_entry_t的初始化
THREAD_TABLE_ENTRY( ardrone_control, 20 )
THREAD_TABLE_ENTRY( navdata_update, 20 )
//以上线程是sdk提供的，这里是对threadTable[]的初始化
END_THREAD_TABLE



HANDLE hStdout =  NULL;  /* Handle to the output console */
CONSOLE_SCREEN_BUFFER_INFO csbiInfo;				/* Information about the output console */



void ARWin32Demo_SetConsoleCursor(int x,int y)
{
    if (hStdout==NULL) hStdout=GetStdHandle(STD_OUTPUT_HANDLE);

    if (hStdout != INVALID_HANDLE_VALUE)
    {
        GetConsoleScreenBufferInfo(hStdout, &csbiInfo);
        csbiInfo.dwCursorPosition.X=x;
        csbiInfo.dwCursorPosition.Y=y;
        SetConsoleCursorPosition(hStdout,csbiInfo.dwCursorPosition);
    }
}

void ARWin32Demo_AcquireConsole(int x,int y)
{
    vp_os_mutex_lock(&consoleMutex);
}
void ARWin32Demo_ReleaseConsole(int x,int y)
{
    vp_os_mutex_unlock(&consoleMutex);
}