/*
 Websocket Server Protocol

 This example demostrate a simple echo server.
 It demostrate how the library <WebSocketProtocol.h> works
 and how to handle the state changes.

 dependent library:WIZNET <Ethernet.h>,ETH_Extra.h

 created  14 Feb 2015
 by MDM Tseng
 */

#include <SPI.h>
#define private public //dirty trick
#include <Ethernet.h>
#undef private
#include <WebSocketProtocol.h>
#include "utility/w5100.h"
#include "utility/socket.h"



#include <ETH_Extra.h>
#define DEBUG_
#ifdef DEBUG_
#define DEBUG_print(A, ...) Serial.print(A,##__VA_ARGS__)
#define DEBUG_println(A, ...) Serial.println(A,##__VA_ARGS__)
#else
#define DEBUG_print(A, ...)
#define DEBUG_println(A, ...)
#endif

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(10, 0, 0, 52);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

EthernetServer server(5213);
WebSocketProtocol WSP[4];

char buff[1024];
char *buffiter;

char retPackage[1024];

WebSocketProtocol::WPFrameInfo retframeInfo;//={.opcode = 1, .isMasking = 0, .isFinal = 1 };
void setup() {
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();
  Serial.begin(57600);

  DEBUG_print("Chat server address:");
  DEBUG_println(Ethernet.localIP());
  setRetryTimeout(4, 1000);
}

unsigned int counter2Pin = 0;
byte LiveClient = 0;

void RECVWebSocketPkg(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
{  
  if (WProt->getPkgframeInfo().opcode == 2)//binary data
  {
    DEBUG_print("Binary");
    RECVWebSocketPkg_binary( WProt, client, RECVData);
    return;
  }
  unsigned int RECVDataL=WProt->getPkgframeInfo().length;
  retframeInfo.opcode = 1; //text
  retframeInfo.isMasking = 0; //no masking on server side
  retframeInfo.isFinal = 1; //is Final package
  //If RECVData is text message, it will end with '\0'
  
  
  char *dataBuff = retPackage + 8;//write data after 8 bytes(8 bytes are for header)
  unsigned int MessageL = sprintf(dataBuff, "RECV:%s", RECVData); //echo
  unsigned int totalPackageL;
  char* retPkg = WProt->codeFrame(dataBuff, MessageL, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
  WProt->getClientOBJ().write(retPkg, totalPackageL);
}


void RECVWebSocketPkg_binary(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
{

  unsigned int RECVDataL=WProt->getPkgframeInfo().length;
  retframeInfo.opcode = 2; //binary
  retframeInfo.isMasking = 0; //no masking on server side
  retframeInfo.isFinal = 1; //is Final package

  char *dataBuff = retPackage + 8;//write data after 8 bytes(8 bytes are for header)
  dataBuff[0]=0x55;//add data in front
  memcpy ( dataBuff+1,RECVData, RECVDataL );//echo
  unsigned int totalPackageL;
  char* retPkg = WProt->codeFrame(dataBuff, RECVDataL+1, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
  WProt->getClientOBJ().write(retPkg, totalPackageL);
  //DoRECVData( WProt, client, RECVData);
}







void loop() {
  // wait for a new client:

  EthernetClient client = server.available();

  if (!client)
  {
    if (LiveClient)
    {

      if (counter2Pin++ > 1000)//check client still alive periodically
      {
        PingAllClient();
        clearUnreachableClient();
        counter2Pin = 0;
      }
      delay(10 >> LiveClient);
    }
    else
      delay(1);
    return;
  }


  buffiter = buff;
  unsigned int  KL = 0;

  unsigned int PkgL =  client.available();
  KL = PkgL;
  recv(client._sock, (uint8_t*)buffiter, PkgL);//get raw data
  WebSocketProtocol* WSPptr  = findFromProt(client);
  if (WSPptr == null)
  {
    client.stop();
    return;
  }
  client = WSPptr->getClientOBJ();


  char *recvData = WSPptr->processRecvPkg(buff, KL);//Check/process is the websocket PKG

  byte frameL = WSPptr->getPkgframeInfo().length; //get frame

  if (WSPptr->getState() == WS_HANDSHAKE)//On hand shaking
  {
    DEBUG_print("WS_HANDSHAKE::");
    DEBUG_println(client._sock);
    client.print(buff);
    return;
  }
  if (WSPptr->getRecvOPState() == WSOP_CLOSE)//websocket close
  {

    DEBUG_print("Normal close::");
    DEBUG_println(client._sock);
    client.stop();
    WSPptr->rmClientOBJ();
    return;
  }
  if (WSPptr->getRecvOPState() == WSOP_UNKNOWN)
    //not websocket package. might be AJAX or normal TCP data
    //handle it by yourself.
  {
    DEBUG_print("unusual close::");
    DEBUG_println(client._sock);
    client.print(WSPptr->codeSendPkg_endConnection(buff));

    client.stop();
    WSPptr->rmClientOBJ();
    return;
  }

  // Normal websocket section
  // client::WSPptr
  // recv Data::recvData
  RECVWebSocketPkg(WSPptr, &client, recvData);

}
void clearUnreachableClient()
{
  LiveClient = 0;
  for (byte i = 0; i < 4; i++)
  {
    EthernetClient Rc = WSP[i].getClientOBJ();
    if (Rc && Rc.status() == 0x00)
    {
      DEBUG_print("clear timeout sock::");
      DEBUG_println(Rc._sock);
      Rc.stop();
      WSP[i].rmClientOBJ();

    }
    else if (Rc)
      LiveClient++;
  }
}
void PingAllClient()
{
  for (byte i = 0; i < 4; i++)
    if (WSP[i].getClientOBJ())
    {
      //byte SnIR = ReadSn_IR(WSP[i].getClientOBJ()._sock);

      testAlive(WSP[i].getClientOBJ()._sock);
    }
}
byte countConnected()
{
  byte C = 0;
  for (byte i = 0; i < 4; i++)
    if (WSP[i].getClientOBJ())
      C++;
  return C;
}

WebSocketProtocol* findFromProt(EthernetClient client)
{

  LiveClient = 0;
  for (byte i = 0; i < 4; i++)
  {
    EthernetClient Rc = WSP[i].getClientOBJ();
    if (Rc == client)
      return WSP + i;
  }

  for (byte i = 0; i < 4; i++)
  {
    if (!WSP[i].getClientOBJ())
    {

      WSP[i].setClientOBJ(client);

      LiveClient = i;
      byte ii = i;
      for (; ii < 4; ii++)if (WSP[ii].getClientOBJ())LiveClient++;

      DEBUG_print("new socket:::");
      DEBUG_print(client._sock);
      DEBUG_print('/');
      DEBUG_println(LiveClient);
      // OnClientsChange();
      return WSP + i;
    }
  }
  return null;
}

