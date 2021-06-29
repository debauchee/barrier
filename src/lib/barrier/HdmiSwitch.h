#pragma once

#include <string>
using std::string;

class HdmiSwitch
{
private:
  static void ParsePortSettings(const string & portSettings);
  static void OpenComPort();
  static void SendStringToComPort(const string & command);
public:
  static void SendCommand(const string & command, const string & portSettings);
};
