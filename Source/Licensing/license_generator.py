#!/usr/bin/env python3
"""
Aura License Key Generator v2 (Gehaertet)

Key-Format: AURA-CCCC-MMMM-SSSSSSSS (23 Zeichen)
  CCCC = Kundennummer (4 Zeichen alphanumerisch)
  MMMM = Machine-ID Prefix (erste 4 Zeichen der Machine-ID)
  SSSSSSSS = MD5-HMAC Signatur (8 Hex-Zeichen)

Die Machine-ID wird im Plugin unter "Lizenz" angezeigt.
Der Kunde muss diese beim Kauf angeben.

Verwendung:
  python license_generator.py                     # Interaktiver Modus
  python license_generator.py --machine AB12CD34 --customer 0001
  python license_generator.py --batch 10 --machine AB12CD34

WICHTIG: Das Secret muss EXAKT mit assembleSecret() in LicenseManager.cpp
uebereinstimmen. Bei Aenderung dort MUSS auch hier angepasst werden!
"""

import hashlib
import argparse
import sys

# === GEHEIMES SECRET ===
# MUSS identisch sein mit LicenseManager::assembleSecret()
# XOR-Fragmente in C++ ergeben: "AuRa_Eq_2026_LiCeNsE_kEy_SeCrEt_V2"
SECRET = "AuRa_Eq_2026_LiCeNsE_kEy_SeCrEt_V2"


def compute_signature(customer_id: str, machine_id: str) -> str:
    """
    Berechnet die MD5-HMAC Signatur (identisch mit C++ Implementierung).
    Input: SECRET + "-" + customerID + "-" + machineID
    Output: Erste 8 Hex-Zeichen des MD5-Hash (uppercase)
    """
    input_str = f"{SECRET}-{customer_id}-{machine_id}"
    md5_hash = hashlib.md5(input_str.encode('utf-8')).hexdigest()
    return md5_hash[:8].upper()


def generate_key(customer_id: str, machine_id: str) -> str:
    """
    Generiert einen gueltigen, machine-gebundenen Lizenz-Key.
    
    Args:
        customer_id: 4-stellige Kundennummer (z.B. "0001")
        machine_id:  8-stellige Machine-ID aus dem Plugin (z.B. "AB12CD34")
    
    Returns:
        Key im Format AURA-CCCC-MMMM-SSSSSSSS (23 Zeichen)
    """
    cid = customer_id.upper().ljust(4)[:4]
    mid = machine_id.upper()
    
    if len(mid) < 4:
        raise ValueError(f"Machine-ID muss mindestens 4 Zeichen haben (erhalten: '{mid}')")
    
    machine_prefix = mid[:4]
    signature = compute_signature(cid, mid)
    
    key = f"AURA-{cid}-{machine_prefix}-{signature}"
    assert len(key) == 23, f"Key-Laenge falsch: {len(key)} statt 23"
    return key


def validate_key(key: str, machine_id: str) -> tuple:
    """
    Validiert einen Lizenz-Key gegen eine Machine-ID.
    
    Returns:
        (is_valid: bool, message: str)
    """
    key = key.upper().strip()
    machine_id = machine_id.upper().strip()
    
    # Format-Check
    if not key.startswith("AURA-"):
        return False, "Key muss mit 'AURA-' beginnen"
    
    if len(key) != 23:
        return False, f"Ungueltige Key-Laenge (erwartet 23, erhalten {len(key)})"
    
    if key[4] != '-' or key[9] != '-' or key[14] != '-':
        return False, "Ungueltige Trennzeichen-Positionen"
    
    # Felder extrahieren
    customer_id = key[5:9]
    machine_prefix = key[10:14]
    signature = key[15:23]
    
    # Machine-ID pruefen
    if machine_prefix != machine_id[:4]:
        return False, f"Machine-ID stimmt nicht ({machine_prefix} != {machine_id[:4]})"
    
    # Signatur pruefen
    expected = compute_signature(customer_id, machine_id)
    if signature != expected:
        return False, f"Signatur ungueltig (erwartet {expected}, erhalten {signature})"
    
    return True, f"Key gueltig fuer Machine {machine_id} (Kunde: {customer_id})"


def main():
    parser = argparse.ArgumentParser(
        description="Aura License Key Generator v2",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Beispiele:
  %(prog)s --machine AB12CD34 --customer 0001
  %(prog)s --machine AB12CD34 --batch 10
  %(prog)s --validate AURA-0001-AB12-F3A2B1C8 --machine AB12CD34
  %(prog)s   (interaktiver Modus)
""")
    parser.add_argument('--machine', '-m', type=str, help='8-Zeichen Machine-ID aus dem Plugin')
    parser.add_argument('--customer', '-c', type=str, default='0001', help='4-Zeichen Kundennummer (Standard: 0001)')
    parser.add_argument('--batch', '-b', type=int, help='Anzahl Keys fuer eine Machine-ID generieren')
    parser.add_argument('--validate', '-v', type=str, help='Key zum Validieren')
    parser.add_argument('--start', '-s', type=int, default=1, help='Start-Kundennummer fuer Batch (Standard: 1)')
    
    args = parser.parse_args()
    
    # Validierungsmodus
    if args.validate:
        if not args.machine:
            print("FEHLER: --machine erforderlich fuer Validierung")
            sys.exit(1)
        is_valid, msg = validate_key(args.validate, args.machine)
        print(f"{'GUELTIG' if is_valid else 'UNGUELTIG'}: {msg}")
        sys.exit(0 if is_valid else 1)
    
    # Batch-Modus
    if args.batch and args.machine:
        print(f"\n{'='*60}")
        print(f"Batch: {args.batch} Keys fuer Machine {args.machine.upper()}")
        print(f"{'='*60}\n")
        
        for i in range(args.start, args.start + args.batch):
            cid = f"{i:04d}"
            key = generate_key(cid, args.machine)
            print(f"  Kunde {cid}: {key}")
        
        sys.exit(0)
    
    # Einzelner Key
    if args.machine:
        key = generate_key(args.customer, args.machine)
        print(f"\nGenerierter Key: {key}")
        print(f"  Kunde:      {args.customer.upper().ljust(4)[:4]}")
        print(f"  Machine:    {args.machine.upper()}")
        print(f"  Key-Laenge: {len(key)}")
        
        # Selbst-Validierung
        is_valid, msg = validate_key(key, args.machine)
        print(f"  Validierung: {msg}")
        sys.exit(0)
    
    # Interaktiver Modus
    print("=" * 60)
    print("  Aura License Key Generator v2 (Machine-Bound)")
    print("=" * 60)
    print()
    print("  Key-Format: AURA-CCCC-MMMM-SSSSSSSS")
    print("    CCCC = Kundennummer")
    print("    MMMM = Machine-ID Prefix")
    print("    SSSSSSSS = MD5-Signatur")
    print()
    print("  Befehle:")
    print("    g <machine_id> [kundennr]  - Key generieren")
    print("    v <key> <machine_id>       - Key validieren")
    print("    b <machine_id> <anzahl>    - Batch generieren")
    print("    quit                        - Beenden")
    print("=" * 60)
    
    while True:
        try:
            user_input = input("\n> ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        
        if not user_input:
            continue
        
        if user_input.lower() == 'quit':
            break
        
        parts = user_input.split()
        cmd = parts[0].lower()
        
        try:
            if cmd == 'g' and len(parts) >= 2:
                mid = parts[1]
                cid = parts[2] if len(parts) > 2 else "0001"
                key = generate_key(cid, mid)
                print(f"  Key: {key}")
                is_valid, msg = validate_key(key, mid)
                print(f"  Validierung: {msg}")
            
            elif cmd == 'v' and len(parts) >= 3:
                is_valid, msg = validate_key(parts[1], parts[2])
                print(f"  {'GUELTIG' if is_valid else 'UNGUELTIG'}: {msg}")
            
            elif cmd == 'b' and len(parts) >= 3:
                mid = parts[1]
                count = int(parts[2])
                for i in range(1, count + 1):
                    key = generate_key(f"{i:04d}", mid)
                    print(f"  {i:03d}: {key}")
            
            else:
                print("  Unbekannter Befehl. Hilfe: g/v/b/quit")
        
        except Exception as e:
            print(f"  FEHLER: {e}")


if __name__ == "__main__":
    main()
