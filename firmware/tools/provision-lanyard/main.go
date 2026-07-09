package main

import (
	"crypto/rand"
	"encoding/hex"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"time"
)

const (
	magic       = "ILSS"
	partSize    = 0x1000
	defaultOff  = 0x1E0000
	serialLen   = 32
	secretLen   = 32
	uuidLen     = 16
)

func uuidv7() ([]byte, error) {
	b := make([]byte, uuidLen)
	// time_ms (48 bits) + ver/rand
	ms := uint64(time.Now().UnixMilli())
	b[0] = byte(ms >> 40)
	b[1] = byte(ms >> 32)
	b[2] = byte(ms >> 24)
	b[3] = byte(ms >> 16)
	b[4] = byte(ms >> 8)
	b[5] = byte(ms)
	if _, err := rand.Read(b[6:]); err != nil {
		return nil, err
	}
	b[6] = (b[6] & 0x0f) | 0x70 // version 7
	b[8] = (b[8] & 0x3f) | 0x80 // variant RFC 4122
	return b, nil
}

func buildBlob(serial string, brand byte, secret, deviceUUID []byte) ([]byte, error) {
	if len(secret) != secretLen {
		return nil, fmt.Errorf("secret must be %d bytes", secretLen)
	}
	if len(deviceUUID) != uuidLen {
		return nil, fmt.Errorf("uuid must be %d bytes", uuidLen)
	}
	blob := make([]byte, partSize)
	for i := range blob {
		blob[i] = 0xff
	}
	copy(blob[0:4], []byte(magic))
	copy(blob[4:20], deviceUUID)
	sb := make([]byte, serialLen)
	copy(sb, []byte(serial))
	copy(blob[20:52], sb)
	blob[52] = brand
	copy(blob[53:85], secret)
	return blob, nil
}

func main() {
	port := flag.String("port", "", "Serial port (e.g. /dev/ttyACM0 or COM3). Required unless -out-only.")
	serial := flag.String("serial", "", "Device serial string (required), e.g. ILSS-LY-0001")
	brand := flag.Int("brand", 1, "Brand enum (1 = Honeywell)")
	secretHex := flag.String("secret", "", "64-char hex factory secret (random if omitted)")
	uuidHex := flag.String("uuid", "", "32-char hex device UUID (random UUIDv7 if omitted)")
	offset := flag.Uint64("offset", defaultOff, "Flash offset of ble_prov partition")
	baud := flag.Int("baud", 921600, "esptool baud rate")
	outPath := flag.String("out", "", "Also write the 4KiB blob to this file")
	outOnly := flag.Bool("out-only", false, "Only write -out file; do not flash")
	flashApp := flag.Bool("flash-app", false, "Run idf.py flash for the app before writing ble_prov")
	chip := flag.String("chip", "esp32s3", "esptool chip target")
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, `provision-lanyard — program the ble_prov factory identity partition

WHEN TO USE
  Run once per physical unit at factory / first bring-up, or after a full
  flash erase that wiped the ble_prov partition. Day-to-day app flashes
  (idf.py flash) do NOT require re-provisioning — ble_prov is a separate
  partition at 0x1E0000.

WHAT IT WRITES (4 KiB at ble_prov)
  magic "ILSS" | UUIDv7 (16) | serial (32) | brand (1) | secret (32)

EXAMPLES
  # Provision identity + secret onto a connected board
  go run ./tools/provision-lanyard -port /dev/ttyACM0 -serial ILSS-LY-0001

  # Generate blob only (no flash)
  go run ./tools/provision-lanyard -serial ILSS-LY-0001 -out-only -out ble_prov.bin

  # Flash firmware app, then provision
  go run ./tools/provision-lanyard -port /dev/ttyACM0 -serial ILSS-LY-0001 -flash-app

Or use the bash wrapper: ./tools/provision-lanyard.sh ...

`)
		flag.PrintDefaults()
	}
	flag.Parse()

	if *serial == "" {
		flag.Usage()
		os.Exit(2)
	}
	if !*outOnly && *port == "" {
		fmt.Fprintln(os.Stderr, "error: -port is required unless -out-only")
		os.Exit(2)
	}
	if *outOnly && *outPath == "" {
		fmt.Fprintln(os.Stderr, "error: -out is required with -out-only")
		os.Exit(2)
	}

	var secret []byte
	var err error
	if *secretHex != "" {
		secret, err = hex.DecodeString(*secretHex)
		if err != nil || len(secret) != secretLen {
			fmt.Fprintln(os.Stderr, "error: -secret must be 64 hex chars (32 bytes)")
			os.Exit(2)
		}
	} else {
		secret = make([]byte, secretLen)
		if _, err = rand.Read(secret); err != nil {
			fmt.Fprintln(os.Stderr, "error: rng:", err)
			os.Exit(1)
		}
	}

	var deviceUUID []byte
	if *uuidHex != "" {
		deviceUUID, err = hex.DecodeString(*uuidHex)
		if err != nil || len(deviceUUID) != uuidLen {
			fmt.Fprintln(os.Stderr, "error: -uuid must be 32 hex chars (16 bytes)")
			os.Exit(2)
		}
	} else {
		deviceUUID, err = uuidv7()
		if err != nil {
			fmt.Fprintln(os.Stderr, "error: uuid:", err)
			os.Exit(1)
		}
	}

	blob, err := buildBlob(*serial, byte(*brand), secret, deviceUUID)
	if err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		os.Exit(1)
	}

	fmt.Printf("serial=%s\n", *serial)
	fmt.Printf("uuid=%s\n", hex.EncodeToString(deviceUUID))
	fmt.Printf("secret=%s\n", hex.EncodeToString(secret))
	fmt.Printf("brand=%d\n", *brand)
	fmt.Printf("offset=0x%X\n", *offset)

	if *outPath != "" {
		if err := os.WriteFile(*outPath, blob, 0o644); err != nil {
			fmt.Fprintln(os.Stderr, "error writing -out:", err)
			os.Exit(1)
		}
		fmt.Printf("wrote %s (%d bytes)\n", *outPath, len(blob))
	}

	if *outOnly {
		fmt.Println("Provisioning blob written (flash skipped).")
		return
	}

	toolsDir, err := os.Getwd()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	// go run . from tools/provision-lanyard → firmware is parent of tools/
	fwDir := toolsDir
	if filepath.Base(toolsDir) == "provision-lanyard" {
		fwDir = filepath.Clean(filepath.Join(toolsDir, "../.."))
	} else if filepath.Base(toolsDir) == "tools" {
		fwDir = filepath.Dir(toolsDir)
	}

	if *flashApp {
		cmd := exec.Command("idf.py", "-p", *port, "flash")
		cmd.Dir = fwDir
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		fmt.Println("→ idf.py flash")
		if err := cmd.Run(); err != nil {
			fmt.Fprintln(os.Stderr, "idf.py flash failed:", err)
			os.Exit(1)
		}
	}

	tmp, err := os.CreateTemp("", "ilss-ble-prov-*.bin")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	tmpName := tmp.Name()
	defer os.Remove(tmpName)
	if _, err := tmp.Write(blob); err != nil {
		tmp.Close()
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	tmp.Close()

	esptool := findEsptool()
	args := []string{
		"--chip", *chip,
		"--port", *port,
		"--baud", fmt.Sprintf("%d", *baud),
		"write_flash",
		fmt.Sprintf("0x%X", *offset),
		tmpName,
	}
	fmt.Println("→", esptool, args)
	cmd := exec.Command(esptool, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		fmt.Fprintln(os.Stderr, "esptool failed:", err)
		os.Exit(1)
	}
	fmt.Println("Provisioning complete.")
}

func findEsptool() string {
	for _, c := range []string{"esptool.py", "esptool"} {
		if p, err := exec.LookPath(c); err == nil {
			return p
		}
	}
	return "esptool.py"
}
