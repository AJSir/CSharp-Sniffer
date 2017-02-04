/*  Archivo: CRawSocket.pp
    Fecha: Octubre 2004 
    Autor: Angel J. Hern�ndez M
    Lenguaje: Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 13.10.3077 for 80x86
    E-m@il: angeljesus14@hotmail.com
    URL: http://groups.msn.com/desarrolladoresmiranda 

    Notas: Implementaci�n de CRawSocket.h
*/

#include "stdafx.h"
#include "CRawSocket.h"

using namespace Sniffer;
using namespace System::Net;
using namespace System::Threading;

/************************************************************************************
* Clase CCommon (M�todos de soporte a clase CRawSocket   *
**************************************************************************************/
/* M�todo encargado de obtener una direcci�n IP a partir de un entero largo */
String *CCommon::GetIPAddressFromLong(u_long ipaddress) {
	char *parsedip;
	String *retval;
	in_addr str;

	if (ipaddress !=  0) {
		str.S_un.S_addr = ipaddress;
		parsedip = inet_ntoa(str);
		retval = Convert::ToString(parsedip);
        } else throw new System::Exception(EXCEPTIONMSG1);

	return retval;
}

/* M�todo encargado de convertir un String* en un char*  */
char *CCommon::ConvertFromStringToCharPtr(String *exprtoconvert) {
    /* Para obtener memoria hacemos uso de la funci�n malloc() (CRT) sin embargo pudimos haber utilizado
        la funci�n VirtualAlloc() del API. La cual obtiene memoria en las p�ginas de memoria virtual. Ambas 
        funciones regresan void *
        Por ejemplo:
        VirtualAlloc(NULL, ((exprtoconvert -> Length + 1) * 2) , MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    */
    const wchar_t __pin *temp = PtrToStringChars(exprtoconvert);
    char *retval = (char *) malloc((exprtoconvert -> Length + 1) * 2);
    wcstombs(retval, temp, (exprtoconvert -> Length + 1) * 2);

    return retval;
}


/* M�todo encargado de recuperar informaci�n detallada de los adaptadores de red instalados en el equipo */
String *CCommon::GetNetworkAdapterAddresses()[] {
    PIP_ADAPTER_INFO info = NULL;
    u_long buffer = 0;
    int nError = 0;

    /* Obtenemos la informaci�n detallada */
    nError = GetAdaptersInfo(info, &buffer);

     switch(nError) {
        case NOERROR:
            m_adapterinfo = info;
            break;
        case ERROR_NO_DATA:
            MessageBox(NULL, NOADAPTER, MSGBOXCAPTION, MB_ICONEXCLAMATION | MB_OK);
            break;
        case ERROR_NOT_SUPPORTED:
            MessageBox(NULL, NOSUPPORTED, MSGBOXCAPTION, MB_ICONEXCLAMATION | MB_OK);
            break;
        case ERROR_BUFFER_OVERFLOW:
            /* Hacemos espacio para la estructura en base al valor devuelto por buffer */
            info = (PIP_ADAPTER_INFO) malloc(buffer);
            if (GetAdaptersInfo(info, &buffer) == NOERROR) 
                m_adapterinfo = info; 
            break;
     }

    /* Obtenemos las direcciones IP disponibles en el equipo mediante el  m�todo GetHostByName */
    IPHostEntry *entry = Dns::GetHostByName(Dns::GetHostName());
    String *retval[] = new String*[entry->AddressList->Count];

    for (int x = 0; x < entry->AddressList->Count; x++)
        retval[x] = entry->AddressList[x]->ToString();

    return retval;
}

/* Muestra cuadro de acerca de Windows */      
void CCommon::ShowWindowsAboutBox(IntPtr hWnd, String *szApp, String *szOtherStuff, IntPtr hIcon) {
    ShellAbout((HWND) hWnd.ToPointer(), ConvertFromStringToCharPtr(szApp),
                    ConvertFromStringToCharPtr(szOtherStuff),  (HICON)  hIcon.ToPointer());
}

/* Buscamos la informaci�n correspondiente a una direcci�n IP espec�fica */
PIP_ADAPTER_INFO CCommon::QueryListForSpecificAddress(String *ip) {
    PIP_ADAPTER_INFO queried = m_adapterinfo,  retval = NULL;
    PIP_ADDR_STRING ptraddress = NULL;
    System::Text::StringBuilder *sb = new System::Text::StringBuilder();
    char *selected = NULL, *address = ConvertFromStringToCharPtr(ip);
    int control = 1;

    if (queried != NULL) {
        ptraddress = &(queried->IpAddressList);
      do {
            sb->Append(ptraddress->IpAddress.String);
            selected = ConvertFromStringToCharPtr(sb->ToString());

            if (strcmp(address, selected) == 0) {
                retval = queried;
                control = 0;
            } else {
                queried = queried->Next;
                ptraddress = &(queried->IpAddressList);
                sb->Remove(0, sb->ToString()->Length);
            }
       } while(control);
    }
    return retval;
}

/* M�todo encargado de Parsear la estructura con informaci�n de la tarjeta de red */
 CCommon::TreeviewDetails CCommon::ParseAdapterInfoData(String *ip) {
    PIP_ADAPTER_INFO selected = QueryListForSpecificAddress(ip);
    TreeviewDetails retval;
    char buffer[50];
   
    if (selected != NULL) {
        /* MAC Address  */
        sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", selected->Address[0], selected->Address[1],
                  selected->Address[2], selected->Address[3], selected->Address[4], selected->Address[5]);
        retval.MAC = Convert::ToString(buffer);

        /* Descripci�n  */
        retval.AdapterName = Convert::ToString(selected->Description);
        
        /* Tipo de Adaptador  */
        sprintf(buffer, "%d", selected->Type);
        retval.Type = Convert::ToString(buffer);

        /* Subm�scara de red  */
        sprintf(buffer, "%s", selected->IpAddressList.IpMask.String);
        retval.NetworkSubMask = Convert::ToString(buffer);

        /*  Gateway */
        sprintf(buffer, "%s", selected->GatewayList.IpAddress.String);
        retval.Gateway = Convert::ToString(buffer);

        /* WINS  */
        sprintf(buffer, "%s", selected->PrimaryWinsServer.IpAddress.String);
        retval.WINS = selected->HaveWins ? Convert::ToString(buffer) : NOTAVAILABLE;

        /* DHCP  */
        sprintf(buffer, "%s", selected->DhcpServer.IpAddress.String);
        retval.DHCP = selected->DhcpEnabled ? Convert::ToString(buffer) : NOTAVAILABLE;

        /* Direcci�n IP */
        retval.IPAddress = ip;
    }
    return retval;
 } 


 /************************************************************************************
 * Clase  CRawSocket (Implementaci�n del RawSocket            *
 **************************************************************************************/

/* Constructor */
CRawSocket::CRawSocket(String *ipaddress, int port) {
    m_filteredprotocol = CCommon::NONE;
	m_socket = new Socket(AddressFamily::InterNetwork, SocketType::Raw, ProtocolType::IP);
	m_socket->Blocking = false;
	m_socket->Bind(new IPEndPoint(IPAddress::Parse(ipaddress), port));
	m_socket->SetSocketOption(SocketOptionLevel::IP, SocketOptionName::HeaderIncluded, 1);
	SetSocketIOControl();
}

/* M�todo encargado de "disponer" el objeto al destruirse */
void CRawSocket::Dispose() {
    if (m_socket != NULL)
        m_socket->Shutdown(SocketShutdown::Both);

    if (m_listener != NULL) {
        try {
            m_listener->Abort();
            } catch (Exception *e) {String *dummy = e->Message;}
        }
    }

/* M�todo encargado de establecer las opciones de bajo nivel del socket */
int CRawSocket::SetSocketIOControl() {
	int retval = 0;
	Byte input[] = {1, 0, 0, 0};
	Byte output[] = new Byte[4];
	m_socket->IOControl(CCommon::SIO_RCVALL, input, output);

	return (((int) retval = output[0] + output[1] + output[2] + output[3]));
}

/* M�todo encargado de iniciar o continuar el procesamiento del hilo que escucha los paquetes entrantes */
void CRawSocket::StartListening() {
    if (m_listener != NULL)
        m_listener->Resume();
    else {
        m_listener = new Thread(new ThreadStart(this, ListenerThread));
        m_listener->Priority = ThreadPriority::Lowest;
        m_listener->Start();
    }
	if (OnStartSniffing != NULL) OnStartSniffing(this);
}


/* M�todo encargado de suspender o pausar el hilo que escucha los paquetes entrantes */
void CRawSocket::StopListening() {
	if (m_listener != NULL) m_listener->Suspend();
	if (OnStopSniffing != NULL) OnStopSniffing(this);
}


/* Proceso ejecutado en el hilo que escucha los paquetes */
void CRawSocket::ListenerThread() {
	int bytesread;
    Byte buffer __gc[] = new Byte[CCommon::MAXBUFFERSIZE];

	for(;;) {
		IAsyncResult *ar = m_socket->BeginReceive(buffer, 0, buffer->Length, SocketFlags::None,
			                                                             new AsyncCallback(this, DummyCallBackFunction), this);

		bytesread = m_socket->EndReceive(ar);
		m_bytesreceived += bytesread;
		PacketReceiver(buffer, bytesread);
	}
}

/* M�todo encargado de desglozar y disparar el evento de paquete recibido */
void CRawSocket::PacketReceiver(Byte packet[], int length) {
    CCommon::PacketContent packetdata = ParseIPHeader(packet, length);
    CCommon::Protocols proto =  packetdata.Protocol->Equals("TCP") ?
                                              CCommon::Protocols::TCP : packetdata.Protocol->Equals("UDP") ?
                                              CCommon::Protocols::UDP : packetdata.Protocol->Equals("ICMP") ?
                                              CCommon::Protocols::ICMP : CCommon::Protocols::NONE;

    if ((m_filteredprotocol == CCommon::Protocols::NONE ||  
        m_filteredprotocol == proto) && OnPacketReceived != NULL)
          OnPacketReceived(this, packetdata); 
}

/* Este m�todo no hace absolutamente nada... Necesario para hacer la llamada as�ncrona desde el hilo */
void CRawSocket::DummyCallBackFunction(IAsyncResult *ar) {}

/* M�todo encargado de parsear o interpretar el encabezado del paquete */
CCommon::PacketContent CRawSocket::ParseIPHeader(Byte packet[], int length) {
	short src_port = 0, dst_port = 0;
	System::Text::StringBuilder *src_address, *dst_address;
    CCommon::PacketContent retval;
	src_address = new System::Text::StringBuilder();
	dst_address = new System::Text::StringBuilder(); 

    Byte *packeddata =   &packet[0] ;
    CCommon::IPHeader *ipheader =  (CCommon::IPHeader *)   *(Byte *) &packeddata;

    retval.HeaderLength = (Int32) ((ipheader->verlen & CCommon::HEADEROFFSET) << 2);
    src_port =   *reinterpret_cast<System::Int16 *> (&packeddata[retval.HeaderLength]); 
    dst_port =   *reinterpret_cast<System::Int16 *> (&packeddata[retval.HeaderLength + 2]); 
	src_address->Append(CCommon::GetIPAddressFromLong((int) ipheader->source)); 
	src_address->Append(":");
	src_address->Append(Convert::ToString(IPAddress::NetworkToHostOrder(src_port))); 
	dst_address->Append(CCommon::GetIPAddressFromLong((int) ipheader->destination)); 
	dst_address->Append(":");
	dst_address->Append(Convert::ToString(IPAddress::NetworkToHostOrder(dst_port)));
    retval.DataLength = length - retval.HeaderLength;
    retval.PacketLength = length;
    retval.Protocol = (__try_cast<Object * > (__box((CCommon::Protocols) ((int) ipheader->protocol))))->ToString(); 
    retval.SourceIP = src_address->ToString();
    retval.TargetIP = dst_address->ToString();
    retval.RawPacket = packet;

	return retval;
}

/* M�todo encargado de obtener las estad�sticas de la red (ICMP, TCP, IP, UDP) */
CCommon::NetworkStats CRawSocket::GetNetworkStats() {
    CCommon::NetworkStats retval;
    MIB_ICMP icmp;
    MIB_IPSTATS ip;
    MIB_TCPSTATS tcp;
    MIB_UDPSTATS udp;

    GetIcmpStatistics(&icmp);
    GetIpStatistics(&ip);
    GetTcpStatistics(&tcp);
    GetUdpStatistics(&udp);

    retval.dwMsgs = icmp.stats.icmpOutStats.dwMsgs;
    retval.dwEchos = icmp.stats.icmpOutStats.dwEchos;
    retval.dwDestUnreachs = icmp.stats.icmpOutStats.dwDestUnreachs;
    retval.dwInReceives = ip.dwInReceives;
    retval.dwInDelivers = ip.dwInDelivers;
    retval.dwOutDiscards = ip.dwOutDiscards;
    retval.dwInSegs = tcp.dwInSegs;
    retval.dwOutSegs = tcp.dwOutSegs;
    retval.dwNumConns = tcp.dwNumConns;
    retval.dwInDatagrams = udp.dwInDatagrams;
    retval.dwOutDatagrams = udp.dwOutDatagrams;

    return retval;
}