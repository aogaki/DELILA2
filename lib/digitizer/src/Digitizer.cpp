#include "Digitizer.hpp"

#include <iostream>

#include "DigitizerFactory.hpp"

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Constructor/Destructor
// ============================================================================

Digitizer::Digitizer() : fDigitizerImpl(nullptr) {}

Digitizer::~Digitizer() = default;

// ============================================================================
// Main Lifecycle Methods
// ============================================================================

bool Digitizer::Initialize(const ConfigurationManager &config)
{
  try {
    fDigitizerImpl = DigitizerFactory::CreateDigitizer(config);
    if (!fDigitizerImpl) {
      return false;
    }
    return fDigitizerImpl->Initialize(config);
  } catch (const std::exception &e) {
    std::cerr << "Error creating digitizer: " << e.what() << std::endl;
    return false;
  }
}

bool Digitizer::Configure()
{
  if (!fDigitizerImpl) return false;
  return fDigitizerImpl->Configure();
}

bool Digitizer::StartAcquisition()
{
  if (!fDigitizerImpl) return false;
  return fDigitizerImpl->StartAcquisition();
}

bool Digitizer::StopAcquisition()
{
  if (!fDigitizerImpl) return false;
  return fDigitizerImpl->StopAcquisition();
}

// ============================================================================
// Data Access
// ============================================================================

std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
Digitizer::GetEventData()
{
  if (!fDigitizerImpl) return nullptr;
  return fDigitizerImpl->GetEventData();
}

// ============================================================================
// Device Information
// ============================================================================

const nlohmann::json &Digitizer::GetDeviceTreeJSON() const
{
  if (!fDigitizerImpl) {
    static const nlohmann::json empty = nlohmann::json::object();
    return empty;
  }
  return fDigitizerImpl->GetDeviceTreeJSON();
}

void Digitizer::PrintDeviceInfo()
{
  if (!fDigitizerImpl) return;
  fDigitizerImpl->PrintDeviceInfo();
}

// ============================================================================
// Control Methods
// ============================================================================

bool Digitizer::SendSWTrigger()
{
  if (!fDigitizerImpl) return false;
  return fDigitizerImpl->SendSWTrigger();
}

bool Digitizer::CheckStatus()
{
  if (!fDigitizerImpl) return false;
  return fDigitizerImpl->CheckStatus();
}

// ============================================================================
// Getters
// ============================================================================

uint64_t Digitizer::GetHandle() const
{
  if (!fDigitizerImpl) return 0;
  return fDigitizerImpl->GetHandle();
}

uint8_t Digitizer::GetModuleNumber() const
{
  if (!fDigitizerImpl) return 0;
  return fDigitizerImpl->GetModuleNumber();
}

}  // namespace Digitizer
}  // namespace DELILA