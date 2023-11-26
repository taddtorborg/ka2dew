//##  LISTEN TO L4 TRAFFIC.C
//## This utility opens a telnet session to the node for port 1, then inquires as to which ports exist.
//## Next it opens a telnet session to every other port.
//## each telnet session is polled sequentially and all traffic that is not a node broadcast, or from TNC, is logged and displayed.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <sys/time.h>

#define RESET  "\e[0m"
#define RED    "\e[31m"
#define GREEN  "\e[30;42m"  // Black/Green
#define YELLOW "\e[33m"
#define YELRED "\e[31;43m" //Red/Yellow
#define BLUE   "\e[34m"
#define WHITE  "\e[37m"
#define BLUW   "\e[37;44m"


#define true 1
#define false 0

#define DEBUG false
#define BUF_LEN 1024

//change log
// v1  based on V9 of listen_to_port.c
// v2  Recognize this node's callsign-SSID instead of just the callsign.
// v3  modify to work with 7 telnet sessions instead of just 1
// v4  change the name to l4listen
// v5  add retries for one of the OpenTelnetSession read cases - rearrange colors and print the "Open a LISTEN session" in the appropriate color.
#define VERSIONNUMBER 5

static unsigned int debugConvertToNumeric_G=false;
#define MAXNUMBEROFPORTS 10
                                                                 //yellow     //cyan          //white         //red         //green         //purple       //blue
static char portColor_L[MAXNUMBEROFPORTS][10] = { "no port 0", "\e[93m\e[1m", "\e[96m\e[1m", "\e[97m\e[1m", "\e[91m\e[1m", "\e[92m\e[1m", "\e[95m\e[1m", "\e[94m\e[1m" };
static char callsign_L[10];
static unsigned int maxPortNumber_L;

#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })





#define GETSTRINGLENGTH_MAXLEN 1000
unsigned int GetStringLength(char *passed_ptrToString)
{
    unsigned int len_l;
    unsigned int index_l;
    index_l = 0;
    len_l   = 0;

    if( passed_ptrToString[0] == 0 )
    {
        return 0;
    }

    while( (index_l < GETSTRINGLENGTH_MAXLEN) && (len_l == 0) )
    {
        if( passed_ptrToString[index_l] == 0 )
        {
            len_l = index_l;
        }
        index_l++;
    }
    if( index_l >= GETSTRINGLENGTH_MAXLEN )
    {
        printf("%u %s FAIL: string never ended\n", __LINE__, __FUNCTION__);
        return 0;
    }
    return(len_l);
}


//############################################################
// SEND TEXT TO NODE
// returns TRUE if good execution or FALSE if failure
unsigned int SendTextToNode(char *passed_sendThis, int passed_handle)
{
    unsigned int len_l;
    unsigned int index_l;
    index_l = 0;
    len_l   = 0;

    len_l = GetStringLength(passed_sendThis);
    if( passed_sendThis[0] == 0 )
    {
        printf("%u %s FAIL: parameter length string may be 0 length\r\n", __LINE__, __FUNCTION__);
        return false;
    }
    if(DEBUG)
    {
        printf("%u %s sending to telnet, socket=%i", __LINE__, __FUNCTION__, passed_handle);
        for(unsigned int x=0; x<len_l; x++)
        {
            char         a;
            unsigned int b;
            if((x&0x0F) == 0)
            {
                printf("\n%02X:", x);
            }
            a=passed_sendThis[x];
            b=(unsigned int)a;
            if((b>=' ') && (b<='z'))
            {
                printf("%c", a);
            }
            else
            {
                printf("<%02X>", b);
            }
        }
        printf("\n\r");
    }
    if( send(passed_handle, passed_sendThis, len_l, 0) < 0 )
    {
        printf("\r\n%u %s Failed to send %u chars to socket.\r\n", __LINE__, __FUNCTION__, len_l);
        fprintf(stderr, "Failed to send data to socket.\r\n");
        return false;
    }
    if(DEBUG)
    {
        printf("\n\r%u %s end\n\r", __LINE__, __FUNCTION__);
    }
    return true;
}



//################################################################
// READ TEXT FROM NODE
// returns TRUE if everything went ok.  FALSE if fail
//
// This function filters the incoming data for non-printable characters.  Printables include <CR><LF> and space through 0x7E

unsigned int ReturnTextFromNode(char *passed_returnBuffer, int passed_handle, int passed_bufLen)
{
    unsigned int len_l;                                   //derived length of the data from the telnet connection.
    unsigned int wndx_l;                                  //write index to the return buffer.
    unsigned int rndx_l;                                 //read Index from telnet received data.
    char bufferFromTelnet_l[BUF_LEN];                     //this buffer is used to receive the raw data from the Telnet session
    unsigned int done_l;
    unsigned int index_l;
    if(DEBUG) printf("%u %s entry -- socket=%i\r\n", __LINE__, __FUNCTION__, passed_handle);

    memset(passed_returnBuffer, 0, passed_bufLen);        //pased_returnBuffer where we return text from this function
    memset(bufferFromTelnet_l, 0, BUF_LEN);
    if(recv(passed_handle, bufferFromTelnet_l, passed_bufLen, 0)< 0)
    {
        fprintf(stderr, "%u Failed to receive data from socket.\r\n", __LINE__);
        return false;
    }

    //calculate the length of the received ata
    len_l   = 0;
    rndx_l  = 0;
    wndx_l  = 0;
    done_l  = false;
    while(!done_l)
    {
        char         a;
        unsigned int b;
        a=bufferFromTelnet_l[rndx_l++];
        b=(unsigned int)a;
        if((b>=' ') && (b<=0x7E))
        {
            passed_returnBuffer[wndx_l++]=a;
            len_l++;
        }
        else if((b==0x0D) || (b==0x0A))
        {
            passed_returnBuffer[wndx_l++]=a;
            len_l++;
        }
        else if(b==0)
        {
            passed_returnBuffer[wndx_l++]=0;
            passed_returnBuffer[wndx_l++]=0;
            passed_returnBuffer[wndx_l++]=0;
            done_l = true;
        }
    }

    if(DEBUG)
    {
        printf("%u %s len of message from server =%u,  useful len for caller=%u\r\n", __LINE__, __FUNCTION__, rndx_l, len_l);
        printf("%u %s The message from the server was:", __LINE__, __FUNCTION__);
        for(unsigned int x=0; x<len_l; x++)
        {
            char         a;
            unsigned int b;
            if((x&0x0F) == 0)      //if end of a line of 16 characters, print a carriage return and index
            {
                printf("\n%02X:", x);
            }
            a=passed_returnBuffer[x];
            b=(unsigned int)a;
            if((b>=' ') && (b<=0x7E))
            {
                printf("%c", a);
                passed_returnBuffer[wndx_l++] = a;
            }
            else
            {
                printf("<%02X>", b);
            }
        }

        printf("\r\n%u %s --- end message from server\n", __LINE__, __FUNCTION__);
        //printf("%u %s Hex Copy:", __LINE__, __FUNCTION__);
        //{
        //    unsigned int lineLenCount_l;
        //    lineLenCount_l = 0;
        //    for(unsigned int x=0; x<len_l; x++)
        //    {
        //        if((x&0x0F) == 0)
        //        {
        //            printf("\r\n%02X:", x);
        //        }
        //        printf("%02X ", passed_returnBuffer[x]);
        //    }
        //}
        //printf("\r\n");
        printf("%u %s exit\r\n", __LINE__, __FUNCTION__);
    }

    return true;
}


//################################################################
// READ TEXT FROM NODE
// returns TRUE if everything went ok.  FALSE if fail

unsigned int ReturnTextFromNodeReturnImmediately(char *passed_returnBuffer, int passed_handle, int passed_bufLen)
{
    unsigned int len_l;                                   //derived length of the data from the telnet connection.
    unsigned int wndx_l;                                  //write index to the return buffer.
    unsigned int rndx_l;                                 //read Index from telnet received data.
    char bufferFromTelnet_l[BUF_LEN];                     //this buffer is used to receive the raw data from the Telnet session
    unsigned int done_l;
    unsigned int index_l;
    memset(bufferFromTelnet_l, 0, BUF_LEN);
    memset(passed_returnBuffer, 0, passed_bufLen);
    if(recv(passed_handle, bufferFromTelnet_l, BUF_LEN, MSG_DONTWAIT)< 0)
    {
        //fprintf(stderr, "%u Failed to receive data from socket.\n", __LINE__);
        return false;
    }

    {
        unsigned int len_l;   //this is an output of this funciton
        if(DEBUG) printf("%u %s The message from the server was:", __LINE__, __FUNCTION__);
        unsigned int maxLen_l = min(BUF_LEN, passed_bufLen)-4;    //leave enough room for terminating nulls
        unsigned int x;
        done_l = false;
        x=0;
        while(!done_l)
        {
            char         a;
            unsigned int b;
            if(DEBUG && ((x&0x0F) == 0))      //if end of a line of 16 characters, print a carriage return and index
            {
                printf("\n%02X:", x);
            }
            a=bufferFromTelnet_l[x];
            b=(unsigned int)a;
            if((b>=' ') && (b<=0x7E))
            {
                if(DEBUG) printf("%c", a);
                passed_returnBuffer[wndx_l++] = a;
                len_l++;
            }
            else if((b==0x0A) || (b==0x0D))
            {
                if(DEBUG) printf("%c", a);
                passed_returnBuffer[wndx_l++] = a;
                len_l++;
            }
            else if((b == 0) || (maxLen_l <= x) || (maxLen_l < wndx_l))
            {
                passed_returnBuffer[wndx_l++] = 0;    //use several nulls to make the end of good data be obvious to hex inspection.
                passed_returnBuffer[wndx_l++] = 0;
                passed_returnBuffer[wndx_l++] = 0;
                done_l=true;
            }
            else
            {
                if(DEBUG) printf("<%02X>", b);
            }
            x++;
        }

        if(DEBUG) printf("\r\n%u %s --- end message from server\n", __LINE__, __FUNCTION__);
        //printf("%u %s Hex Copy:\r\n", __LINE__, __FUNCTION__);
        //{
        //    unsigned int lineLenCount_l;
        //    lineLenCount_l = 0;
        //    for(unsigned int x=0; x<len_l; x++)
        //    {
        //        if((x&0x0F) == 0)
        //        {
        //            printf("\r\n%02X:", x);
        //        }
        //        printf("%02X ", passed_returnBuffer[x]);
        //    }
        //}
        //printf("\r\n");
    }

    return true;
}

//###############################################################################
// CONVERT TEXT STRING TO NUMERIC
// Takes a passed pointer to a ASCII text string of ASCII digits from ‘0’ to ‘9’ and
// ending in a null 0 value, and converts the number into an unsigned 32bit integer.
// This function converts anything that is not a null or an ASCII digit to an ASCII '0'.
// So, 123XXX will be turned into result=123000.
unsigned int ConvertTextStringToNumeric(char *textString_p)
{
    unsigned int numberOfDigits_l;     //Count the characters first, and put the result here.
    unsigned int result_l = 0;         //This gets the value we'll be returning to the caller

    if(debugConvertToNumeric_G) printf("ConvertTextString ToNumeric(%s): START\r\n", textString_p);
    numberOfDigits_l = 0;
    while(textString_p[numberOfDigits_l] != 0)      // if isn't a null, it counts as a character
    {
        numberOfDigits_l++;
    }

    if(debugConvertToNumeric_G) printf("ConvertTextString ToNumeric(%s): numberOfDigits_l=%i\r\n", textString_p, numberOfDigits_l);
    if(numberOfDigits_l == 0)            //if there are no characters, return a 0
    {
        result_l = 0;
    }
    else
    {
        unsigned int index_l = 0;
        char oneLetter_l;                 //this gets a letter from the passed string.
        if(debugConvertToNumeric_G) printf("number of digits = %i\r\n", numberOfDigits_l);
        while(index_l < numberOfDigits_l)
        {
            if(debugConvertToNumeric_G) printf("ConvertTextString ToNumeric(%s): Top of loop, index=%i, result=%i\r\n", textString_p, index_l, result_l);
            result_l *= 10;                        //Coming back o the top of this while loop, shift the result to the left by *10.
            if(debugConvertToNumeric_G) printf("result*10 = %i\r\n", result_l);
            oneLetter_l = textString_p[index_l];            //get the next letter from the ASCII string
            if(oneLetter_l == ' ')                          //spaces count as a "0"
            {
                oneLetter_l = '0';
            }
            if((oneLetter_l < '0') || (oneLetter_l > '9'))    //letters which are not digits, count as a "0"
            {
                if(debugConvertToNumeric_G) printf("ConvertTextString ToNumeric(%s): digit at index %i found to be out-of-range = %c %02X\r\n", textString_p, index_l, oneLetter_l, oneLetter_l);
                oneLetter_l = '0';
            }
            result_l += oneLetter_l - '0';            //convert the ASCII digit to an integer from 0 to 9, and add it to the result
            index_l++;                              // one more ASCII digit processed
        }
    }

    if(debugConvertToNumeric_G) printf("ConvertTextString ToNumeric(%s): EXIT result=%i\r\n", textString_p, result_l);

    return (result_l);
}



//##############################################################################
// GET_RTC_TIME_IN_SECONDS
unsigned int GetRtcTimeInSeconds(void)
{
   struct timeval curTime_l;
   gettimeofday(&curTime_l, NULL);
   return(curTime_l.tv_sec);
}

//#############################################################################
static void PrintTimeString(void)
{
   char timeString_l[128];
   struct timeval curTime_l;
   char a[200];
   memset(a, 0, sizeof(a));


   //now format a good log message with date stamp
   gettimeofday(&curTime_l, NULL);
   strftime(timeString_l, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime_l.tv_sec));
   //printf("curTime.tv_sec=%i\n\r", curTime_l.tv_sec);
   //printf("TimeString=%s\r\n", timeString);

   printf("%s", timeString_l);
}


//##############################################################################################################################################################
// OPEN TELNET SESSION
//
// If we succeed in connecting to the G8BPQ host, then return the socket number.
// If we fail, then print an error message and exit this utility.
//
//The way the TELNET server login on the G8BPQ node works, for port 8010, is that you connect to it and it sends you a bunch
//of welcome crap and then asks you to log in.  The login procedure asks for your username and then a specific password
//configured for your username in the BPQ32.CFG file.   For some generic G8BPQ operators (not TARPN) there is a list of
//usernames who are allowed to log-in over the Internet.  Each user-name has an associated password.
// LOGINPROMPT=Login Name:
// PASSWORDPROMPT=Suppled Password:
// USER=fredsmith,maryqueenofscotts123,k1qrm,user,SYSOP
// USER=charlesbaker,donotseemerabbit,kf4qsy,user,SYSOP

//The TARPN configuration for G8BPQ LINBPQ node creates a custom LOGIN prompt which actually gives us the "operator callsign".
// The one and only USER configured for the Telnet server is the local operator, since we don't do Internet.  Also since we don't
//do Internet, we don't need a password at all.  But there is no way to eliminate the process so we make the password "p" on every node.
// The TARPN version of the config file is more like this
// LOGINPROMPT=opcallsign:
// PASSOWRDPROMPT=type p<ENTER>
// USER=opcallsign,p,opcallsign,user,SYSOP
int OpenTelnetSession(void)
{
    int socket_l;
    unsigned short port;
    struct sockaddr_in server;       /* Telnet server address */
    unsigned int lengthOfCallsignField_l;
    char receiveBuffer_l[BUF_LEN];
    char callsignForLogIn_l[10];
    /* create stream socket using TCP */
    socket_l = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_l == -1)
    {
        printf("Socket was not created.\n");
        exit(2);
    }
    //printf("%u %s socket=%i\r\n", __LINE__, __FUNCTION__, socket_l);
    //Create a sockaddr_in structure with Server address info to be passed to the connect() API call

    port = (unsigned short)atoi("8010");                            /* get the port number for the G8BPQ node generic Telnet server */
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr("127.0.1.1");                //For Raspberry PI, this is the local IP address

    /* Connect to the Telnet server */
    if(connect(socket_l, &server, sizeof(server))< 0)
    {
        printf("Failed to connect to socket.  trying again in 5 seconds\n");
        sleep(5);
        if(connect(socket_l, &server, sizeof(server))< 0)
        {
            printf("Failed to connect to socket.  trying a 3rd time in 5 seconds\n");
            sleep(5);
            if(connect(socket_l, &server, sizeof(server))< 0)
            {
                printf("Failed to connect to socket.  trying a 4th time in 5 seconds\n");
                sleep(5);
                if(connect(socket_l, &server, sizeof(server))< 0)
                {
                    printf("Failed to connect to socket.  trying a 5th time in 5 seconds\n");
                    sleep(5);
                    if(connect(socket_l, &server, sizeof(server))< 0)
                    {
                        printf("Failed to connect to socket.  Is the NODE software running?\n");
                        exit(3);
                    }
                }
            }
        }
    }
    sleep(1);                                                               //The reason for the sleep() calls is so the Telnet server response is already available when we make the recv() call
    if(DEBUG)printf("%u %s read buffer for first prompts - socket_l=%i\r\n", __LINE__, __FUNCTION__, socket_l);
    if(!ReturnTextFromNode(receiveBuffer_l,  socket_l, BUF_LEN)) exit(1);    //Get the Welcome message and login-prompt and toss it in the hopper
    //Part of what the Telnet server sent when we logged on is the LOGIN prompt.  Since all we have to do to get JUST the LOGIN prompt,
    //all by itself, is send a blank return, send one.  That'll get us a relatively clear copy of the prompt.
    {
        unsigned int retryCount_l;
        unsigned int succeed_l;
        retryCount_l = 5;
        succeed_l    = false;
        while((succeed_l==false) && (retryCount_l > 0))
        {
            retryCount_l--;
            printf("%u %s Send <CR> to get log-in prompt - socket_l=%i retryCount_l=%u\r\n", __LINE__, __FUNCTION__, socket_l, retryCount_l);
            ReturnTextFromNodeReturnImmediately(receiveBuffer_l,  socket_l, BUF_LEN);  //get and toss away excess.
            if(!SendTextToNode("\r", socket_l)) exit(1);
            sleep(1);                                                               //The reason for the sleep() calls is so the Telnet server response is already available when we make the recv() call
            if(!ReturnTextFromNode(receiveBuffer_l,  socket_l, BUF_LEN))    //Get user-name prompt, (our callsign preceeded by 0x0A and followed by a ':'), from the Telnet server
            {
                printf("%u %s  retry timeout=%u\r\n", __LINE__, __FUNCTION__, retryCount_l);
            }
            else
            {
                succeed_l = true;
            }
            printf("%u %s bottom of while loop\ succeed=%u  retry=%u\n", __LINE__, __FUNCTION__,succeed_l,retryCount_l);
        }
        if(!succeed_l)
        {
            printf("%u %s ABORT program after failing to get buffer from port while waiting for answer to our <CR> at log-in prompt\r\n", __LINE__, __FUNCTION__);
            exit(1);
        }
    }
    if(DEBUG)printf("%u %s callsign response should be in here: %s\r\n", __LINE__, __FUNCTION__, receiveBuffer_l);
    //Figure out what user callsign the node is prompting us with.  (we don't know because this application could run on anybody's node)
    //Actually, we could read it from node.ini file, and maybe that would be interesting to do later.  In the mean time,
    //the user-name PROMPT will be our log-in name so we just need to feed it back to the Telnet server in answer to the login prompt.
    {
        char chewedBuffer_l[20];
        unsigned int chew_l = 0;
        unsigned int output_l = 0;

        // First make sure we chew up any unneeded characters off the start of the string.
        while((chew_l < 20) && (receiveBuffer_l[chew_l] != 0) )
        {
            if(receiveBuffer_l[chew_l] > ' ')
            {
                chewedBuffer_l[output_l] = receiveBuffer_l[chew_l];
                output_l++;
            }
            chew_l++;
        }

        //Now count the length of the remaining callsign, terminated by the ':' character
        lengthOfCallsignField_l = 0;
        while((lengthOfCallsignField_l < 8) && (chewedBuffer_l[lengthOfCallsignField_l] != ':') )
        {
            lengthOfCallsignField_l++;
        }

        //Check to see that we got a valid callsign length.  If we did not, just print it and exit back to Linux
        if(( lengthOfCallsignField_l > 6 ) || (lengthOfCallsignField_l < 4 ) )
        {
            printf("%u %s FAIL - Callsign field didn't work out.  len=%u value=%02X %02X %02X %02X %02X %02X\n", __LINE__, __FUNCTION__, lengthOfCallsignField_l, receiveBuffer_l[0], receiveBuffer_l[1], receiveBuffer_l[2], receiveBuffer_l[3], receiveBuffer_l[4], receiveBuffer_l[5] );
            exit(1);
        }

        //We seem to have a text string of the proper length to be a callsign.  Save it off.
        for(unsigned int x=0; x < lengthOfCallsignField_l; x++)
        {
            callsignForLogIn_l[x] = chewedBuffer_l[x];
        }
    }
    callsignForLogIn_l[lengthOfCallsignField_l] = '\r';    //terminate the callsign string with a <return> and a <null>
    callsignForLogIn_l[lengthOfCallsignField_l+1] = 0;
    if(DEBUG) printf("callsign from Telnet prompt = %s\r\n", callsignForLogIn_l);


    if(DEBUG) printf("%u %s Send an echo of the callsign, and a carriage return to log-in\r\n", __LINE__, __FUNCTION__);
    if(!SendTextToNode(callsignForLogIn_l, socket_l))                       //answer the LOGIN prompt by sending the callsign obtained just above
    {
        exit(1);
    }
    sleep(1);                                                               //The reason for the sleep() calls is so the Telnet server response is already available when we make the recv() call
    if(!ReturnTextFromNode(receiveBuffer_l,  socket_l, BUF_LEN))     //get PASSWORD prompt from the Telnet server -- we know what it looks like so toss it out.
    {
        printf("%u %s failed to get buffer from port after sending our echo of callsign at log-in prompt\r\n", __LINE__, __FUNCTION__);
        exit(1);
    }

    //Answer the PASSWORD prompt
    if(DEBUG) printf("%u %s Answer the password prompt with p<carriage return>\r\n", __LINE__, __FUNCTION__);
    if(!SendTextToNode("p\r", socket_l))                             //in the BPQ32.CFG file we've set the Telnet password, for just the operator, to 'p'.
    {
        exit(1);
    }
    sleep(1);
    if(!ReturnTextFromNode(receiveBuffer_l,  socket_l, BUF_LEN))     //The node will send us a USER command response once we log in successfully  Get it and toss it out
    {
        printf("%u %s failed to get buffer from port while waiting for answer to our p password\r\n", __LINE__, __FUNCTION__);
        exit(1);
    }
    if(DEBUG) printf("%u %s Rx from log-in=%s\r\n", __LINE__, __FUNCTION__, receiveBuffer_l);
    return socket_l;
}



//#######################################################################################################################################################
//#######################################################################################################################################################
//#######################################################################################################################################################
//#######################################################################################################################################################
//#######################################################################################################################################################
//#######################################################################################################################################################
//#######################################################################################################################################################
//#######################################################################################################################################################
//#######################################################################################################################################################
int main(argc, argv)
int argc;
char **argv;
{
    unsigned int lengthOfCallsignField_l;
    int sock_l[MAXNUMBEROFPORTS];                       /* client socket */
    char transmitBuffer_l[BUF_LEN]; /* data buffer for sending and receiving */
    char receiveBuffer_l[BUF_LEN]; /* data buffer for sending and receiving */


    //printf("--- start of portColor print--\r\n");
    //
    //for(unsigned int x=0; x<MAXNUMBEROFPORTS; x++)
    //{
    //    printf("buffer %u contents=>%s<\r\n", x, portColor_L[x]);
    //    printf("\r\ntest message in color %u \e[0m\r\n", x);
    //}
    //printf("--- end of portColor print--\r\n");
    switch(argc)
    {
        case 1:
            break;

        default:
            printf("l4listen -- version %u\n\rNo parameters are used.\r\n", VERSIONNUMBER);
            printf("This program opens up a telnet session to the local node, watches for a       \r\n");
            printf("callsign prompt.  It answers the callsign prompt with an echo, and then       \r\n");
            printf("sends a 'p' for the password prompt.  The program then sends a PORT command   \r\n");
            printf("which will yield a list of point to point links in the TARPN node.            \r\n");
            printf("The program will open further telnet ports to support LISTEN for the ports    \r\n");
            printf("including port 1 and up to the highest point to point link port listed,       \r\n");
            printf("inclusive and sequential.  LISTEN sessions are then established for each port.\r\n");
            printf("                                                                              \r\n");
            printf("The program watches the G8BPQ diagnostic information that is presented through\r\n");
            printf("the LISTEN results.  Messages that are NETROM <INFO S## are copied and        \r\n");
            printf("presented on STDOUT with ANSI coloring to make it easier to find each port's  \r\n");
            printf("traffic.  Here are some examples:  \r\n");
            sleep(1);
            printf("\e[96m\e[1m---------------------------------------  p2   2023-11-25 23:14:33\r\n");
            printf("NC4FG-2>KA2DEW-2 NET/ROM                              \r\n");
            printf("  NC4FG-9 to KA2DEW-5 ttl 7 cct=0AA3  <INFO S17 R22>: \r\n");
            printf("DWB4IHY-9 WB4IHY V2.3.2 > $TH:IDL11-14-2023 11:14     \r\n");
            printf(" \e[0m\r\n");
            printf("\e[92m\e[1m---------------------------------------  p5   2023-11-25 23:14:33\r\n");
            printf("KA2DEW-3>KA2DEW-2 NET/ROM                               \r\n");
            printf("  KA2DEW-3 to KA2DEW-2 ttl 7 cct=1E37  <INFO S1 R3>:    \r\n");
            printf("FFVC:KA2DEW-3} Routes                                   \r\n");
            printf("> 1 WA4GK-2   200  16!27036 136434 5% 0 0 04:16  0 200\r\n");
            printf("> 3 KA2DEW-2  200  26!25713 1521   5% 0 0 04:21  0 200\r\n");
            printf(" \e[0m\r\n");
            sleep(1);
            printf("\e[96m\e[1m---------------------------------------  p2   2023-11-25 23:15:57\r\n");
            printf("NC4FG-2>KA2DEW-2 NET/ROM \r\n");
            printf("  NC4FG-9 to KA2DEW-5 ttl 7 cct=0AA3  <INFO S18 R22>: \r\n");
            printf("KNC4FG-9 KA2DEW-5 6.0.21.40                           \r\n");
            printf(" \e[0m\r\n");
            printf("\e[96m\e[1m---------------------------------------  p2   2023-11-25 23:15:57r\n");
            printf("NC4FG-2>KA2DEW-2 NET/ROM\r\n");
            printf("  NC4FG-9 to KA2DEW-5 ttl 7 cct=0AA3  <INFO S19 R22>:\r\n");
            printf("DW3KHG-9 W3KHG I see how this could be a problem \r\n");
            printf(" \e[0m\r\n");
            sleep(1);
            printf("\e[93m\e[1m---------------------------------------  p1   2023-11-25 23:17:38\r\n");
            printf("K3PMH-2>KA2DEW-2 NET/ROM\r\n");
            printf("  K3PMH-9 to KA2DEW-5 ttl 7 cct=08BB  <INFO S6 R38>:\r\n");
            printf("DK3PMH-9 K3PMH V2.3.2 > $TH:IDL11-25-2023 21:47     \r\n");
            printf(" \e[0m\r\n");
            sleep(1);
            printf("\e[96m\e[1m---------------------------------------  p2   2023-11-25 23:19:15\r\n");
            printf("NC4FG-2>KA2DEW-2 NET/ROM\r\n");
            printf("  NC4FG-9 to KA2DEW-5 ttl 7 cct=0AA3  <INFO S20 R24>: \r\n");
            printf("DND4L-9 ND4L V2.3.2 > $TH:IDL11-23-2023 15:49 \e[0m\r\n");
            printf("\r\n");
            sleep(1);
            printf("\e[91m\e[1m---------------------------------------  p4   2023-11-25 23:19:55\r\n");
            printf("WO2S-2>KA2DEW-2 NET/ROM\r\n");
            printf("  WO2S-9 to KA2DEW-5 ttl 7 cct=05C8  <INFO S102 R37>:              \r\n");
            printf("[BPQ-6.0.21.40-IHJM$]\r\n");
            printf("Hello Tadd welcome to WO2S bbs. Latest Message is 40, Last listed is 0\r\n");
            printf("$ on WO2S bbs>\r\n");


            exit(1);
    }

    // Capture the port# the user wants us to monitor

    printf("TARPN- l4listen(v%u).  Listen to all L4 traffic on all KISS ports.\n\r", VERSIONNUMBER);
    printf("This program requires sequential ports starting at #1.\n\r");


    //First move is to open up one telnet session to the node and find out how many ports this node has.
    printf("Open Telnet for port #1\r\n");
    sock_l[1]=OpenTelnetSession();

    if(DEBUG) printf("%u %s first telnet session socket=%i/r/n", __LINE__, __FUNCTION__, sock_l[1]);
    //Send the "password" command to the node.  This tells the node we want to do 'privaleged' stuff.
    //Since we are telnetting in to the node from the Raspberry PI itself, it won't challenge us.  It'll just OK and let us through.
    if(!SendTextToNode("password\r", sock_l[1])) exit(1);
    sleep(1);
    if(!ReturnTextFromNode(receiveBuffer_l,  sock_l[1], BUF_LEN)) exit(1);    //Get and toss away the "OK" response to the "PASSWORD" command

    //printf("%u %s test\r\n", __LINE__, __FUNCTION__);
    {
        unsigned int callWriteIndex_l;
        unsigned int colonIndex_l;
        unsigned int huntIndex_l;
        unsigned int done_l;
        char a;
        done_l           = false;
        huntIndex_l      = 0;
        colonIndex_l     = 0;
        callWriteIndex_l = 0;
        while(!done_l)
        {
            callsign_L[callWriteIndex_l]=0;
            a = receiveBuffer_l[huntIndex_l];
            //printf("%u %s receiveBuffer_l[%u]='%c'\r\n", __LINE__, __FUNCTION__, huntIndex_l, receiveBuffer_l[huntIndex_l]);
            if(colonIndex_l == 0)
            {
                if(a==':')
                {
                    colonIndex_l = huntIndex_l;
                }
            }
            else
            {
                if(a=='}')
                {
                    done_l=true;
                }
                else
                {
                    callsign_L[callWriteIndex_l]=a;
                    //printf("%u %s receiveBuffer_l[%u]='%c'  callsign_L[%u]='%c'\r\n", __LINE__, __FUNCTION__, huntIndex_l, receiveBuffer_l[huntIndex_l], callWriteIndex_l, callsign_L[callWriteIndex_l]);
                    callWriteIndex_l++;
                    lengthOfCallsignField_l = callWriteIndex_l;
                }
            }
            huntIndex_l++;
        }
    }
    //printf("%u %s callsign length=%u  callsign=%s\r\n", __LINE__, __FUNCTION__, lengthOfCallsignField_l, callsign_L);


    // Get the PORTS command response which will give us a list of the valid Ports for this node.  We want to know what the last port number will be.
    //  TADD:KA2DEW-2} Ports
    //   1 point to point link
    //   2 point to point link
    //   3 point to point link
    //   4 point to point link
    //   5 point to point link
    //   6 point to point link
    //   7 point to point link
    //  32 Local Telnet
    if(!SendTextToNode("ports\r", sock_l[1])) exit(1);
    sleep(1);
    if(!ReturnTextFromNode(receiveBuffer_l,  sock_l[1], BUF_LEN)) exit(1);    //read the PORTS command response
    printf("Response from ports command = \r\n%s", receiveBuffer_l);
    printf("\r\n");
    {
        unsigned int index_l, done_l;
        index_l = 0;
        done_l  = false;
        while(!done_l && (index_l < 300))
        {
            if((receiveBuffer_l[index_l+9] == 't') && (receiveBuffer_l[index_l+10] == 'o'))
            {
                char a, b;
                int result_l;
                a=receiveBuffer_l[index_l+0];
                b=receiveBuffer_l[index_l+1];
                //printf("%u %s char text port #? >%c%c<\r\n", __LINE__, __FUNCTION__, a, b);
                {
                    char text_l[10];
                    text_l[0]=a;
                    text_l[1]=b;
                    text_l[2]=0;
                    result_l = ConvertTextStringToNumeric(text_l);
                    //printf("%u %s numeric port # result=%u\r\n", __LINE__, __FUNCTION__, result_l);
                    maxPortNumber_L = result_l;
                }
            }
            if(receiveBuffer_l[index_l] == 0)    done_l = true;
            index_l++;
        }
    }
    //printf("%u %s callsign length=%u  callsign=%s\r\n", __LINE__, __FUNCTION__, lengthOfCallsignField_l, callsign_L);

    //Open a telnet to every port.
    if(maxPortNumber_L < 2)
    {
        printf("%u %s Only one port #. Done with open\r\n", __LINE__, __FUNCTION__);
    }
    else
    {
        //printf("%u %s MAX port # is %u\r\n", __LINE__, __FUNCTION__, maxPortNumber_L);
        for(unsigned int x=2; x<=maxPortNumber_L; x++)
        {
            if(DEBUG)printf("%u %s Open Telnet for port #%u\r\n", __LINE__, __FUNCTION__, x);
            printf("Open Telnet for port #%u\r\n", x);
            sock_l[x]=OpenTelnetSession();
            sleep(1);                                                               //The reason for the sleep() calls is so the Telnet server response is already available when we make the recv() call
        }
    }
    //printf("%u %s callsign length=%u  callsign=%s\r\n", __LINE__, __FUNCTION__, lengthOfCallsignField_l, callsign_L);

    /// Now that we have Telnets to all of the ports, go through all of the ports and start a listen session.
    for(unsigned int x=1;x<=maxPortNumber_L;x++)
    {
        printf("%s\e[1mOpen a LISTEN session for port %u\e[0m\r\n", portColor_L[x], x);
        // Send the "lis" command.  This tells the node to start streaming monitored data to this socket.
        sprintf(transmitBuffer_l, "lis %u\r", x);
        if(!SendTextToNode(transmitBuffer_l, sock_l[x])) exit(1);
        sleep(0.2);
        if(!ReturnTextFromNode(receiveBuffer_l,  sock_l[x], BUF_LEN)) exit(1);    //receive and dispose of the "Listening on port" response from the Node
        //printf("%u %s Listen response:%s\r\n", __LINE__, __FUNCTION__, receiveBuffer_l);
    }
    // We need our callsign to figure out whether we're seeing a transmit message or a receive message.
    // Fix callsign so it is all uppercase, like the monitored text will be
    for(unsigned int index_l=0; index_l < lengthOfCallsignField_l; index_l++)
    {
        if(callsign_L[index_l] >= 'Z')
        {
            callsign_L[index_l] -= 0x20;   // only set Letters to upper case.  Numbers get a pass
        }
    }


    // Now go into an infinite loop watching for data streamed from the Telnet server.
    // This data will be from one radio port on the local node.
    while(1)
    {
        unsigned int isTransmit_l;     //this gets 'true' if the callsign of the sending station is our callsign
        unsigned int metaDataToBeIgnored_l;     //this gets 'true' if we see a metadata string in the incoming message
        unsigned int foundInfoStatement_l;     //this gets 'true' if "<INFO S" is in the first 75 bytes of the message
        unsigned int badMyCallsign_l;     //this gets 'true' if the callsign of the sending station is our callsign
        unsigned int index_l;
        char buffer1_l[1000];

        for(unsigned int mainPortNum_l=1; mainPortNum_l<maxPortNumber_L; mainPortNum_l++)
        {
            //printf("%u loop\n", __LINE__);
            if(ReturnTextFromNodeReturnImmediately(buffer1_l,  sock_l[mainPortNum_l], BUF_LEN) > 0)
            {
                //printf("\r\n\r\nprocess incoming buffer=%s\r\n", buffer1_l);

                index_l               = 0;
                isTransmit_l          = true;          //assume this is us for the moment.
                metaDataToBeIgnored_l = false;
                foundInfoStatement_l  = false;
                if((buffer1_l[0] == 'T') && (buffer1_l[1] == 'N') && (buffer1_l[2] == 'C')) metaDataToBeIgnored_l = true;
                if((buffer1_l[5] == '>') && (buffer1_l[6] == 'I') && (buffer1_l[7] == 'D')) metaDataToBeIgnored_l = true;
                if((buffer1_l[6] == '>') && (buffer1_l[7] == 'I') && (buffer1_l[8] == 'D')) metaDataToBeIgnored_l = true;
                if((buffer1_l[7] == '>') && (buffer1_l[8] == 'I') && (buffer1_l[9] == 'D')) metaDataToBeIgnored_l = true;
                if((buffer1_l[8] == '>') && (buffer1_l[9] == 'I') && (buffer1_l[10] == 'D')) metaDataToBeIgnored_l = true;
                if((buffer1_l[9] == '>') && (buffer1_l[10] == 'I') && (buffer1_l[11] == 'D')) metaDataToBeIgnored_l = true;

                if(metaDataToBeIgnored_l == false)
                {
                    //printf("metaDataToBeIgnored is false -- go make sure my callsign isn't sending this\r\n");
                    isTransmit_l = true;          //assume this is us for the moment.
                    index_l      = 0;
                    //printf("%u %s callsign length=%u  callsign=%s\r\n", __LINE__, __FUNCTION__, lengthOfCallsignField_l, callsign_L);
                    while((index_l < lengthOfCallsignField_l) && (isTransmit_l == true))
                    {
                        //printf("%u %s  heard(%u)%c -- this(%u)%c\r\n",  __LINE__, __FUNCTION__, index_l, buffer1_l[index_l], index_l, callsign_L[index_l]);
                        if(callsign_L[index_l] != buffer1_l[index_l])
                        {
                            isTransmit_l = false;      //we know this wasn't us now
                            //printf("unrecognized send callsign.  set isTransmit to false\r\n");
                        }
                        index_l++;
                    }
                    //printf("%u %s isTransmit = %s\r\n", __LINE__, __FUNCTION__,  (isTransmit_l==true?"true":"false"));
                }
                else
                {
                   // printf("metaDataToBeIgnored is true\r\n");
                }
                //printf("%u %s buffer on port %u\r\n", __LINE__, __FUNCTION__, mainPortNum_l);

                // TEST to decide if this is a "<INFO S"   NETROM item.  It is not a transmit, and not a specific metadata address, then print all remaining "<INFO S" messages.
                if((metaDataToBeIgnored_l == true) || (isTransmit_l == true))
                {
                    //if(metaDataToBeIgnored_l == true)
                    //{
                    //    printf("from=TNC or >ID found in message before checking for >INFO\r\n");
                    //}
                    //if(isTransmit_l == true)
                    //{
                    //    printf("This is a tranmsission from me, skip checking for >INFO\r\n");
                    //}
                }
                else
                {
                    //printf("metaDataToBeIgnored is false, and not a transmission from me.  Check for >INFO S\r\n");
                    index_l              = 0;
                    foundInfoStatement_l = false;
                    while((metaDataToBeIgnored_l == false) && (foundInfoStatement_l == false) && (index_l < 75))
                    {
                        if((buffer1_l[index_l] == '<') && (buffer1_l[index_l+1] == 'I') && (buffer1_l[index_l+2] == 'N') && (buffer1_l[index_l+3] == 'F') && (buffer1_l[index_l+4] == 'O') && (buffer1_l[index_l+5] == ' ') &&  (buffer1_l[index_l+6] == 'S'))
                        {
                            foundInfoStatement_l = true;
                            //printf("Found >INFO S in stream\r\n");
                        }
                        else
                        {
                            index_l++;
                        }
                    }
                }


                if((isTransmit_l == false) && (metaDataToBeIgnored_l == false) && (foundInfoStatement_l == true))
                {
                    printf("%s\e[1m---------------------------------------  p%u   ", portColor_L[mainPortNum_l], mainPortNum_l);
                    //printf("b0=%c b1=%c b2=%c b3=%c b4=%c b5=%c b6=%c b7=%c", buffer1_l[0], buffer1_l[1], buffer1_l[2], buffer1_l[3], buffer1_l[4], buffer1_l[5], buffer1_l[6], buffer1_l[7]);
                    //printf(" b8=%c b9=%c b10=%c b11=%c b12=%c b13=%c b14=%c b15=%c\r\n", buffer1_l[8], buffer1_l[9], buffer1_l[10], buffer1_l[11], buffer1_l[12], buffer1_l[13], buffer1_l[14], buffer1_l[15]);
                    PrintTimeString();
                    printf("\r\n%s\e[0m\r\n", buffer1_l);
                }
                else
                {
                    //printf("not printing\r\n");
                    //if(metaDataToBeIgnored_l == true)
                    //{
                    //    printf("metaDataToBeIgnored is TRUE\r\n");
                    //}
                    //else if(isTransmit_l == true)
                    //{
                    //    printf("is-transmit is TRUE\r\n");
                    //}
                    //else if(foundInfoStatement_l == false)
                    //{
                    //    printf("foundInfoStatement_l is FALSE\r\n");
                    //}
                }
            }
        }
        sleep(0.1);
    }

    close(sock_l[1]); /* close the socket */
    exit(0);
}

