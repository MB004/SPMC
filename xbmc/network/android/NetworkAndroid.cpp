/*
 *      Copyright (C) 2016 Christian Browet
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */


#include "NetworkAndroid.h"

#include "android/activity/XBMCApp.h"
#include "android/jni/ConnectivityManager.h"
#include "android/jni/LinkAddress.h"
#include "android/jni/InetAddress.h"
#include "android/jni/RouteInfo.h"

#include "android/jni/WifiManager.h"
#include "android/jni/WifiInfo.h"

#include "utils/StringUtils.h"
#include "utils/log.h"

CNetworkInterfaceAndroid::CNetworkInterfaceAndroid(CJNINetwork network, CJNILinkProperties lp, CJNINetworkInterface intf)
  : m_network(network)
  , m_lp(lp)
  , m_intf(intf)
{
  m_name = m_intf.getName();
}

std::vector<std::string> CNetworkInterfaceAndroid::GetNameServers()
{
  std::vector<std::string> ret;

  CJNIList<CJNIInetAddress> lia = m_lp.getDnsServers();
  for (int i=0; i < lia.size(); ++i)
  {
    ret.push_back(lia.get(i).getHostAddress());
  }

  return ret;
}

std::string& CNetworkInterfaceAndroid::GetName()
{
  return m_name;
}

bool CNetworkInterfaceAndroid::IsEnabled()
{
  CJNIConnectivityManager connman(CXBMCApp::getSystemService(CJNIContext::CONNECTIVITY_SERVICE));
  CJNINetworkInfo ni = connman.getNetworkInfo(m_network);
  if (!ni)
    return false;

  return ni.isAvailable();
}

bool CNetworkInterfaceAndroid::IsConnected()
{
  CJNIConnectivityManager connman(CXBMCApp::getSystemService(CJNIContext::CONNECTIVITY_SERVICE));
  CJNINetworkInfo ni = connman.getNetworkInfo(m_network);
  if (!ni)
    return false;

  return ni.isConnected();
}

bool CNetworkInterfaceAndroid::IsWireless()
{
  CJNIConnectivityManager connman(CXBMCApp::getSystemService(CJNIContext::CONNECTIVITY_SERVICE));
  CJNINetworkInfo ni = connman.getNetworkInfo(m_network);
  if (!ni)
    return false;

  int type = ni.getType();
  return !(type == CJNIConnectivityManager::TYPE_ETHERNET || type == CJNIConnectivityManager::TYPE_DUMMY);
}

std::string CNetworkInterfaceAndroid::GetMacAddress()
{
  auto interfaceMacAddrRaw = m_intf.getHardwareAddress();
  if (xbmc_jnienv()->ExceptionCheck())
  {
    xbmc_jnienv()->ExceptionClear();
    CLog::Log(LOGERROR, "CNetworkInterfaceAndroid::GetMacAddress Exception getting HW address");
    return "";
  }
  if (interfaceMacAddrRaw.size() >= 6)
  {
    return (StringUtils::Format("%02X:%02X:%02X:%02X:%02X:%02X",
                                      (uint8_t)interfaceMacAddrRaw[0],
                                      (uint8_t)interfaceMacAddrRaw[1],
                                      (uint8_t)interfaceMacAddrRaw[2],
                                      (uint8_t)interfaceMacAddrRaw[3],
                                      (uint8_t)interfaceMacAddrRaw[4],
                                      (uint8_t)interfaceMacAddrRaw[5]));
  }
  return "";
}

void CNetworkInterfaceAndroid::GetMacAddressRaw(char rawMac[6])
{
  auto interfaceMacAddrRaw = m_intf.getHardwareAddress();
  if (xbmc_jnienv()->ExceptionCheck())
  {
    xbmc_jnienv()->ExceptionClear();
    CLog::Log(LOGERROR, "CNetworkInterfaceAndroid::GetMacAddress Exception getting HW address");
    return;
  }
  if (interfaceMacAddrRaw.size() >= 6)
    memcpy(rawMac, interfaceMacAddrRaw.data(), 6);
}

bool CNetworkInterfaceAndroid::GetHostMacAddress(unsigned long host, std::string& mac)
{
  // TODO
  return false;
}

std::string CNetworkInterfaceAndroid::GetCurrentIPAddress()
{
  CJNIList<CJNILinkAddress> lla = m_lp.getLinkAddresses();
  if (lla.size() == 0)
    return "";

  int i = 0;
  for (;i < lla.size(); ++i)
  {
    if (lla.get(i).getAddress().getAddress().size() > 4)  // IPV4 only
      continue;
    break;
  }
  if (i == lla.size())
    return "";

  CJNILinkAddress la = lla.get(i);
  return la.getAddress().getHostAddress();
}

std::string CNetworkInterfaceAndroid::GetCurrentNetmask()
{
  CJNIList<CJNILinkAddress> lla = m_lp.getLinkAddresses();
  if (lla.size() == 0)
    return "";

  int i = 0;
  for (;i < lla.size(); ++i)
  {
    if (lla.get(i).getAddress().getAddress().size() > 4)  // IPV4 only
      continue;
    break;
  }
  if (i == lla.size())
    return "";

  CJNILinkAddress la = lla.get(i);

  int prefix = la.getPrefixLength();
  unsigned long mask = (0xFFFFFFFF << (32 - prefix)) & 0xFFFFFFFF;
  return StringUtils::Format("%lu.%lu.%lu.%lu", mask >> 24, (mask >> 16) & 0xFF, (mask >> 8) & 0xFF, mask & 0xFF);
}

std::string CNetworkInterfaceAndroid::GetCurrentDefaultGateway()
{
  CJNIList<CJNIRouteInfo> ris = m_lp.getRoutes();
  for (int i = 0; i < ris.size(); ++i)
  {
    CJNIRouteInfo ri = ris.get(i);
    if (!ri.isDefaultRoute())
      continue;

    CJNIInetAddress ia = ri.getGateway();
    std::vector<char> adr = ia.getAddress();
    return StringUtils::Format("%u.%u.%u.%u", adr[0], adr[1], adr[2], adr[3]);
  }
  return "";
}

std::string CNetworkInterfaceAndroid::GetCurrentWirelessEssId()
{
  std::string ret;

  CJNIConnectivityManager connman(CXBMCApp::getSystemService(CJNIContext::CONNECTIVITY_SERVICE));
  CJNINetworkInfo ni = connman.getNetworkInfo(m_network);
  if (!ni)
    return "";

  if (ni.getType() == CJNIConnectivityManager::TYPE_WIFI)
  {
    CJNIWifiManager wm = CXBMCApp::getSystemService("wifi");
    if (wm.isWifiEnabled())
    {
      CJNIWifiInfo wi = wm.getConnectionInfo();
      ret = wi.getSSID();
    }
  }
  return ret;
}

std::vector<NetworkAccessPoint> CNetworkInterfaceAndroid::GetAccessPoints()
{
  // TODO
  return std::vector<NetworkAccessPoint>();
}

void CNetworkInterfaceAndroid::GetSettings(NetworkAssignment& assignment, std::string& ipAddress, std::string& networkMask, std::string& defaultGateway, std::string& essId, std::string& key, EncMode& encryptionMode)
{
  // Not implemented
}

void CNetworkInterfaceAndroid::SetSettings(NetworkAssignment& assignment, std::string& ipAddress, std::string& networkMask, std::string& defaultGateway, std::string& essId, std::string& key, EncMode& encryptionMode)
{
  // Not implemented
}


/*************************/

CNetworkAndroid::CNetworkAndroid()
{
  RetrieveInterfaces();
}

CNetworkAndroid::~CNetworkAndroid()
{
  for (auto intf : m_interfaces)
    delete intf;
  for (auto intf : m_oldInterfaces)
    delete intf;
}

bool CNetworkAndroid::GetHostName(std::string& hostname)
{
  hostname = CJNIInetAddress::getLocalHost().getHostName();
  return true;
}

std::vector<CNetworkInterface*>& CNetworkAndroid::GetInterfaceList()
{
  CSingleLock lock(m_refreshMutex);
  return m_interfaces;
}

CNetworkInterface* CNetworkAndroid::GetFirstConnectedInterface()
{
  CSingleLock lock(m_refreshMutex);

  for(CNetworkInterface* intf : m_interfaces)
  {
    if (intf->IsEnabled() && intf->IsConnected() && !intf->GetCurrentDefaultGateway().empty())
      return intf;
  }

  return nullptr;
}

bool CNetworkAndroid::PingHost(unsigned long host, unsigned int timeout_ms)
{
  // TODO
  return false;
}

std::vector<std::string> CNetworkAndroid::GetNameServers()
{
  CNetworkInterfaceAndroid* intf = static_cast<CNetworkInterfaceAndroid*>(GetFirstConnectedInterface());
  if (intf)
    return intf->GetNameServers();

  return std::vector<std::string>();
}

void CNetworkAndroid::SetNameServers(const std::vector<std::string>& nameServers)
{
  // Not implemented
}

void CNetworkAndroid::RetrieveInterfaces()
{
  CSingleLock lock(m_refreshMutex);

  // Cannot delete interfaces here, as there still might have references to it
  for (auto intf : m_oldInterfaces)
    delete intf;
  m_oldInterfaces = m_interfaces;
  m_interfaces.clear();

  CJNIConnectivityManager connman(CXBMCApp::getSystemService(CJNIContext::CONNECTIVITY_SERVICE));
  std::vector<CJNINetwork> networks = connman.getAllNetworks();

  for (auto n : networks)
  {
    CJNILinkProperties lp = connman.getLinkProperties(n);
    if (lp)
    {
      CJNINetworkInterface intf = CJNINetworkInterface::getByName(lp.getInterfaceName());
      if (xbmc_jnienv()->ExceptionCheck())
      {
        xbmc_jnienv()->ExceptionClear();
        CLog::Log(LOGERROR, "CNetworkAndroid::RetrieveInterfaces Cannot get interface by name: %s", lp.getInterfaceName().c_str());
        continue;
      }
      if (intf)
        m_interfaces.push_back(new CNetworkInterfaceAndroid(n, lp, intf));
      else
        CLog::Log(LOGERROR, "CNetworkAndroid::RetrieveInterfaces Cannot get interface by name: %s", lp.getInterfaceName().c_str());
    }
    else
      CLog::Log(LOGERROR, "CNetworkAndroid::RetrieveInterfaces Cannot get link properties for network: %s", n.toString().c_str());
  }
}
