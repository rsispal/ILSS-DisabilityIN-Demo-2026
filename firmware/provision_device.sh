#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------
# Defaults
# ---------------------------------------------
OUT_BASE="./out"   # default output root
DEVICE_ID=""
DAYS=3650           # 10 years
BITS=2048           # RSA-2048

# ---------------------------------------------
# Parse CLI arguments
# ---------------------------------------------
usage() {
  echo "Usage: $0 --device-id <id> [--out-dir <path>]"
  echo ""
  echo "Example:"
  echo "  $0 --device-id ILSS12345 --out-dir ./factory_output"
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --device-id)
      DEVICE_ID="$2"
      shift 2
      ;;
    --out-dir)
      OUT_BASE="$2"
      shift 2
      ;;
    -h|--help)
      usage
      ;;
    *)
      echo "Unknown argument: $1"
      usage
      ;;
  esac
done

# ---------------------------------------------
# Validate inputs
# ---------------------------------------------
if [[ -z "${DEVICE_ID}" ]]; then
  echo "❌ ERROR: --device-id is required."
  usage
fi

# ---------------------------------------------
# Construct directory
# ---------------------------------------------
OUT_DIR="${OUT_BASE}/${DEVICE_ID}"
mkdir -p "${OUT_DIR}"

echo "📦 Output directory: ${OUT_DIR}"
echo "🔧 Device ID: ${DEVICE_ID}"
echo ""

# ---------------------------------------------
# File paths
# ---------------------------------------------
PRIMARY_KEY="${OUT_DIR}/${DEVICE_ID}-primary.key.pem"
PRIMARY_CERT="${OUT_DIR}/${DEVICE_ID}-primary.cert.pem"

SECONDARY_KEY="${OUT_DIR}/${DEVICE_ID}-secondary.key.pem"
SECONDARY_CERT="${OUT_DIR}/${DEVICE_ID}-secondary.cert.pem"

PRIMARY_C_FILE="${OUT_DIR}/${DEVICE_ID}_primary_cert.c"
THUMBPRINT_FILE="${OUT_DIR}/${DEVICE_ID}.thumbprints.json"

# ---------------------------------------------
# Functions
# ---------------------------------------------
calc_sha1() {
  openssl x509 -noout -fingerprint -sha1 -in "$1" | cut -d= -f2 | tr -d ':' | tr 'a-f' 'A-F'
}

calc_sha256() {
  openssl x509 -noout -fingerprint -sha256 -in "$1" | cut -d= -f2 | tr -d ':' | tr 'a-f' 'A-F'
}

# ---------------------------------------------
# Generate certificates
# ---------------------------------------------
echo "🔐 Generating PRIMARY certificate..."
openssl req -x509 -nodes -newkey rsa:${BITS} \
  -keyout "${PRIMARY_KEY}" \
  -out "${PRIMARY_CERT}" \
  -days ${DAYS} \
  -subj "/CN=${DEVICE_ID}" \
  -sha256

echo "🔐 Generating SECONDARY certificate..."
openssl req -x509 -nodes -newkey rsa:${BITS} \
  -keyout "${SECONDARY_KEY}" \
  -out "${SECONDARY_CERT}" \
  -days ${DAYS} \
  -subj "/CN=${DEVICE_ID}" \
  -sha256

# ---------------------------------------------
# Calculate thumbprints
# ---------------------------------------------
PRIMARY_SHA1=$(calc_sha1 "${PRIMARY_CERT}")
PRIMARY_SHA256=$(calc_sha256 "${PRIMARY_CERT}")

SECONDARY_SHA1=$(calc_sha1 "${SECONDARY_CERT}")
SECONDARY_SHA256=$(calc_sha256 "${SECONDARY_CERT}")

echo "📎 Azure Thumbprints:"
echo "PRIMARY SHA-1:   ${PRIMARY_SHA1}"
echo "PRIMARY SHA-256: ${PRIMARY_SHA256}"
echo "SECONDARY SHA-1:   ${SECONDARY_SHA1}"
echo "SECONDARY SHA-256: ${SECONDARY_SHA256}"
echo ""

# ---------------------------------------------
# Generate JSON file
# ---------------------------------------------
cat <<EOF > "${THUMBPRINT_FILE}"
{
  "deviceId": "${DEVICE_ID}",
  "primary": {
    "sha1":   "${PRIMARY_SHA1}",
    "sha256": "${PRIMARY_SHA256}"
  },
  "secondary": {
    "sha1":   "${SECONDARY_SHA1}",
    "sha256": "${SECONDARY_SHA256}"
  }
}
EOF

echo "📄 JSON thumbprint file written: ${THUMBPRINT_FILE}"

# ---------------------------------------------
# Generate ESP-IDF certificate header files
# ---------------------------------------------
CERT_DIR="${OUT_DIR}/certs"
mkdir -p "${CERT_DIR}"

# Device certificate header
CERT_C=$(sed 's/"/\\"/g' "${PRIMARY_CERT}" | sed 's/^/"/; s/$/\\n"/')
cat <<EOF > "${CERT_DIR}/azure_device_cert.h"
#pragma once

// Device Certificate for ${DEVICE_ID}
// Generated: $(date '+%Y-%m-%d %H:%M:%S')
// CN=${DEVICE_ID}
// Valid for ${DAYS} days
// SHA-1 Thumbprint: ${PRIMARY_SHA1}
// SHA-256 Thumbprint: ${PRIMARY_SHA256}

const char AZURE_DEVICE_CERT[] =
${CERT_C}
;
EOF

# Device key header
KEY_C=$(sed 's/"/\\"/g' "${PRIMARY_KEY}" | sed 's/^/"/; s/$/\\n"/')
cat <<EOF > "${CERT_DIR}/azure_device_key.h"
#pragma once

// Device Private Key for ${DEVICE_ID}
// Generated: $(date '+%Y-%m-%d %H:%M:%S')
// ⚠️  KEEP THIS FILE SECURE - DO NOT SHARE PUBLICLY ⚠️

const char AZURE_DEVICE_KEY[] =
${KEY_C}
;
EOF

# CA certificate (DigiCert Global Root G2 - Azure IoT Hub CA)
cat <<'EOF' > "${CERT_DIR}/azure_ca_cert.h"
#pragma once

// DigiCert Global Root G2 Certificate
// Required for Azure IoT Hub TLS connections
// Downloaded from: https://cacerts.digicert.com/DigiCertGlobalRootG2.crt.pem
// Valid until: 2038-01-15

const char AZURE_CA_CERT[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
"7uN5vUr47TcdKqX6ahAwDQYJKoZIhvcNAQELBQADggEBAGBm+KUrv2j5HbUu6Yg4\n"
"v7IHo5ol0t1om2YB5U8i4I/gxevTVz9ZRqgs3X78x3edGqrD2Pd3Kqh8Wf5/7oXQ\n"
"UfhYDYFonwGMwnff5yQ+7g3zDvlSe8Yq+Jv4T82b5qTrk/IAs4pMG4qRDj7bjK5\n"
"oHJ6IC5HRQpluBC1Jfpa6o1Y1FvYfpyq/kvWrvhN3ZFCqY5uHg06i9sT4QarxaV\n"
"9BmKtWUcZVyFv4UXxK6HiRVpSdXeqaXpY5u3zi5owdYdQXrY8wejVeKeOf1bGEv\n"
"NO5MPFlqcrjSVph7Ppj/Z8gDX6tlJrSS2s9n7xuy1biB/yf2KZL98R3lwHb4W3es\n"
"lCpdP+KmHPNvPMmcyM0xP6stH/w8Q1WCrMaXuajXp2Kc="
"-----END CERTIFICATE-----\n"
;
EOF

echo "📄 Certificate headers written to: ${CERT_DIR}/"
echo "   - azure_device_cert.h"
echo "   - azure_device_key.h"
echo "   - azure_ca_cert.h"
echo ""
echo "✅ Provisioning complete."
echo ""
echo "📋 Next steps:"
echo "   1. Copy the certificate headers to: src/features/azure-iot/certs/"
echo "   2. Update sdkconfig with your Azure IoT Hub hostname and device ID"
echo "   3. Build and flash your firmware"