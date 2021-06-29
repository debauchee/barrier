
#include "HdmiSwitch.h"
#include "base/Log.h"

static string lastPortSettings("");
string deviceName;
int baudRate, dataBits, parity, stopBits;
static HANDLE comPort = INVALID_HANDLE_VALUE;

void HdmiSwitch::SendCommand(const string & command, const string & portSettings)
{
  try
  {
    LOG((CLOG_INFO "Sending command to HDMI switch. Command: %s, portSettings: %s", command.c_str(), portSettings.c_str()));
    if (comPort == INVALID_HANDLE_VALUE || lastPortSettings.compare(portSettings) != 0)
    {
      ParsePortSettings(portSettings);
      OpenComPort();
    }
    SendStringToComPort(command);
  }
  catch (std::exception & e)
  {
    LOG((CLOG_ERR "Unable to send command to HDMI switch: %s", e.what()));
  }
}

void HdmiSwitch::ParsePortSettings(const string& portSettings)
{
  lastPortSettings = portSettings;
  if (comPort != INVALID_HANDLE_VALUE) CloseHandle(comPort);

  try
  {
    //device name
    int posB = 0;
    int posE = portSettings.find('-');
    string deviceName = string("\\\\.\\") + portSettings.substr(posB, posE - posB);

    //baud rate
    posB = posE + 1;
    posE = portSettings.find('-', posB);
    baudRate = std::stoi(portSettings.substr(posB, posE - posB));

    //data bits
    posB = posE + 1;
    dataBits = portSettings[posB] - '0';

    //parity bit
    posB = posB + 1;
    const string parityLetters = "NOEMS";
    parity = parityLetters.find(portSettings[posB]);

    //stop bits
    posB = posB + 1;
    posE = portSettings.length() + 1;
    string stopBitsString = portSettings.substr(posB, posE - posB);
    stopBits = ONESTOPBIT;
    if (stopBitsString == "1.5") stopBits = ONE5STOPBITS;
    else if (stopBitsString == "2") stopBits = TWOSTOPBITS;

    LOG((CLOG_INFO "COM port settings: device '%s'  baud %d  data %d  parity %d  stop %d", deviceName.c_str(), baudRate, dataBits, parity, stopBits));
  }
  catch (std::exception& e)
  {
    throw std::invalid_argument("Port settings string could not be parsed.");
  }
}

void HdmiSwitch::OpenComPort()
{
  try
  {
    //open com port
    comPort = CreateFile(deviceName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (comPort == INVALID_HANDLE_VALUE) throw std::exception("");

    //set port parameters
    DCB params = { 0 };
    params.DCBlength = sizeof(params);
    if (!GetCommState(comPort, &params)) throw std::exception("");
    params.BaudRate = baudRate;
    params.ByteSize = dataBits;
    params.Parity = parity;
    params.StopBits = stopBits;
    if (!SetCommState(comPort, &params)) throw std::exception("");

    //set timeouts
    COMMTIMEOUTS timeouts = { 50, 10, 50, 10, 50 };
    if (!SetCommTimeouts(comPort, &timeouts)) throw std::exception("");
  }
  catch (std::exception & e)
  {
    if (comPort != INVALID_HANDLE_VALUE) CloseHandle(comPort);
    comPort = INVALID_HANDLE_VALUE;
    throw std::exception("COM port could not be opened.");
  }
}

void HdmiSwitch::SendStringToComPort(const string & command)
{
  DWORD bytes_written = 0;
  if (!WriteFile(comPort, command.c_str(), command.length(), &bytes_written, NULL))
    throw std::exception("Write to COM port failed.");
}

