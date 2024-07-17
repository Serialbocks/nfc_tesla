Tesla Key Card Protocol
=======================

> Researched by Robert Quattlebaum <darco@deepdarc.com>.
> 
> Last updated 2020-02-03.

<p align="center"><img src="https://www.tesla.com/ns_videos/commerce/content/dam/tesla/CAR_ACCESSORIES/MODEL_3/INTERIOR/1131087-00-F_1.jpg" alt="Image of Tesla Key Card" width=320 />
<img src="https://www.tesla.com/ns_videos/commerce/content/dam/tesla/CAR_ACCESSORIES/MODEL_3/INTERIOR/1449859-00-D_3.jpg" alt="Image of Tesla Model 3 Key Fob" width=320 /></p>

> **IMPORTANT NOTE:** *The information in this document does NOT
> enable anyone to clone official Tesla Key Cards or otherwise
> unlock or start a vehicle that they didn't already have the
> ability to unlock or start.*

This document describes the current understanding of the
Tesla Key Card Protocol, as used on the *Tesla Model 3*.
This protocol was determined from observing the pairing and
authentication interactions between the Tesla Key Card (TKC)
and the vehicle, as well as probing the TKC with various commands
to see what would happen.

The basic working subset of commands necessary to pair
and authenticate are well understood and documented here.
There are several other commands that we know are present, but
they are not yet well understood.

The NFC protocol for the [Tesla Model 3 Key Fob][] (TM3KF) and
Tesla smartphone app (TPK) is also covered. In general, any reference
to a TKC also applies to the TM3KF and TPK unless an explicit
exception is noted.

[Tesla Model 3 Key Fob]: https://shop.tesla.com/product/model-3-key-fob

The information presented here was used to implement [GaussKeyCard][],
an open-source Java Card applet that allows you to use a [supported](https://github.com/darconeous/gauss-key-card/wiki/Recommended-Cards)
Java Card to unlock and start vehicles in the same way that you would
with an official Tesla Key Card.

[GaussKeyCard]: https://github.com/darconeous/gauss-key-card

## Terminology

ATS:
Answer To Select. This is a short string of bytes that describe to the reader
how to communicate with the card.

AID:
[Application Identifier](https://en.wikipedia.org/wiki/EMV#Application_selection)

APDU:
[Application Protocol Data Unit](https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit),
name for commands sent to a smart card.

CA:
[Certificate Authority](https://en.wikipedia.org/wiki/Certificate_authority)

ECDH:
[Elliptic-curve Diffie–Hellman](https://en.wikipedia.org/wiki/Elliptic-curve_Diffie–Hellman),
a key-agreement protocol.

FSCI:
The part of the ATS that describes the maximum supported frame size.
Tesla vehicles require an FSCI value of at least 6 (96 bytes).

TPK:
Tesla Phone Key (NFC, only available on Android)

TKC:
Tesla Key Card

TM3KF:
[Tesla Model 3 Key Fob][]

Token:
A secure device used for authentication that is resistant to cloning.

UID:
Unique Identifier

## Background ##

When it comes to access control, there are two related but distinct
concepts: identification and authentication.

*Identification* simply means knowing who/what you are interacting
with. With NFC, the 7-byte unique identifier is often used for
identification purposes since it is defined to be unique. Sometimes
even the older 4-byte UID is used for this purpose.

But this identifier is easily spoofed with easy-to-obtain and
inexpensive hardware. The UID alone offers no cryptographic assurances
that it the authentic, original token.

*Authentication* is an additional step where you cryptographically
verify the identity of the token in a way that only the original token
could satisfy.

## General Description ##

TKCs use 256-bit Elliptic-Curve Cryptography to authenticate to the
vehicle. The specific elliptic curve being used is the NIST P-256
curve, otherwise known as `secp256r1`.

The specific algorithm being used to authenticate the card is a simple
challenge-response using a shared key derived using Elliptic-curve
Diffie–Hellman (ECDH).

When pairing a TKC to a car, the vehicle asks the card for its public
key and then performs a test authentication.

While the TKC protocol does support an attestation certificate, it
does not appear to currently be used. Note that the TPK does NOT
have an attestation certificate.

Each Tesla Key Card has a NFC UID, *which in recent vehicle firmware
versions is ignored*. ~~Testing has confirmed
that two TKCs with the same ECDH key but different NFC UIDs will
appear to the vehicle as two separate cards. However, testing has also
confirmed that two cards with different ECDH keys but the same UID
will *also* appear as two separate cards. Thus, it would appear that
the vehicle identifies each card by both its UID *and* ECDH key.~~

~~Note that the vehicle does not fetch the ECDH public key from the card
when authenticating, so it must be comparing the response from the TKC
against the expected response from all paired TKCs with the same UID.
This is pretty strange, but there isn't a lot of room for alternative
explanations given the observed behavior.~~

> **UPDATE**: Recent versions of the vehicle firmware now fetch the
> public key from the card before sending the challenge, and seem to
> use ONLY this information for identifying a credential. This means
> that it is now MUCH easier to make cloned card *pairs*.
> Note that cloning an existing card remains mostly impossible, but
> prior to this change it was very difficult for your average hacker
> to make two new key cards that would work as clones (becuase it is
> generally difficult to arbitrarilly change the UID of a javacard).
> That is now no longer the case.

## Important: IEEE 14443 Type-A Only

The vehicle will apparently refuse to read 14443 Type-B cards. Only
Type-A cards are compatible.

## Important: Max Frame Size

The vehicle will ignore the FSCI field of the ATS, which means that it
will not attempt to break up larger frames if the indicated FSCI is small (<6).
Specifically, **the card MUST be able to properly handle receiving the
authenticate command (86 bytes) in a single frame**. If a card advertises
an FSCI smaller than 6 then it is unlikely to be able to satisfy this requirement.

For example, smart cards with DESFire EV1 emulation support have an
FSCI of 5, and will unfortunately choke if they receive a frame larger
than 64 bytes. Such cards are not able to be used as Tesla Key Cards.

> NOTE: In earlier versions of this document, this behavior was confused with
> the NFC UID length. It just happened to be the case that most of the
> 4-byte UID cards the author tested also had an FCI of 5. There is no
> limitation of the length of the UID on the card imposed by the vehicle.

## Other potentially relevant details

The Tesla Key Card, as currently sold by Tesla, has the following
potentially relevant properties:

 * IEEE 14443 Type A
 * UID: 7 bytes (except TPK, which may be `01020304` or a randomly selected UID starting with the value `08`)
 * ATQA: 0x4800
 * SAK: 0x20
 * ATS: `057877910200`
    * FSCI: 8 (256 bytes)

The secure element itself is a chip made by NXP (P60) running
a Java Card OS of some sort. It is *NOT* a DESFire chip, it is
a real secure element.

The vehicle will go through the authentication steps for TKCs it
hasn't been paired with.



## Application Identifier (AID)

The primary interface that the vehicle uses to pair and authenticate
TKCs is the AID `7465736c614c6f676963`, or `teslaLogic`. This is the
default selected application on official TKCs.

Note that, strictly speaking, this isn't a valid ISO 7816-5 AID, since
it is not [registered](https://www.ansi.org/other_services/registration_programs/rid)
and it doesn't start with the nibble `0xf` to indicate that it is a
proprietary and unregistered AID. This is likely why the vehicle
actually tries to use `f465736c614c6f676963` first, which is the AID for
the TPK. In any case, the use of the AID `7465736c614c6f676963` is
sufficiently unique that it is unlikely to cause problems in practice.

The TPK AID `f465736c614c6f676963` appears to be treated the same way as
its non-7816-5-compilant cousin.

The full `teslaLogic` AID on official TKCs is `7465736C614C6F67696330303201`,
or `teslaLogic002` followed by the byte `0x01`. `002` is assumed to be the version.
On the TM3KF, the full AID is `teslaLogic005` with no trailing bytes.

No FCI is returned when the application is selected on an official TKC or TM3KF.
In some cases having an FCI present causes the car to reject the card. In other
cases it seems to accept the card. Additional research is needed to figure out
more details.

There is also a `teslaStore` applet (`teslaStore002` for TKC, `teslaStore003`
for TM3KF). However, it does not appear to be selectable.

## Quick-Reference APDUs

* Select `teslaLogic` AID
   * Cards/Fobs: `00a404000a 7465736c614c6f676963`
   * Alternate: `00a404000a f465736c614c6f676963`
* Get Public ECDH Key (secp256r1)
   * `8004000000`
* Authentication Challenge (ECDH)
   * `8011000051 [VEHICLE-PUBLIC-KEY] [16B-CHALLENGE]`
* Get Form Factor
   * `80140000`
* Get Version Info 
   * `80170000`
* Self Test?
   * `80010000`

## Important Commands

The following proprietary (CLA=0x80) APDU instructions are used by
the vehicle during authentication or pairing, or were discovered
by probing:

### INS 0x04: Get Public Key

Field | Value | Notes
------|-------|---------------
CLA   | 0x80  | 
INS   | 0x04  | 
P1    | 0x00-0x03 | Key Identifier
P2    | 0x00  |
Lc    | —     |
Data  | —     |
Le    | 0x00  |

This command returns a public key for one of the keys in the TKC/TM3KF.
**The TPK currently only supports KeyID 0.**

The `KeyID` is a number between 0 and 3. Only 0x00 appears
to be currently used by the vehicle.

No other arguments are currently known.

The observed format of the response (not including success code `9000`) is:

byte 1| bytes 2-33 | bytes 34-65
------|------------|--------------
 0x04 | EC Point X | EC Point Y

### INS 0x06: Get Certificate

Field | Value | Notes
------|-------|---------------
CLA   | 0x80  | 
INS   | 0x06  | 
P1    | 0x00-0x04 | Certificate Identifier
P2    | 0x00  |
Lc    | —     |
Data  | —     |
Le    | 0x000000  | Extended-lenth response

This command returns one of (up to) five X.509 certificates,
one for each `KeyID` and one "root".

**This command is not currently supported on the TPK.**
This command is not currently used by the vehicle during
pairing or authentication.

On the Tesla Key Card, only two certificates are available via this
command: 0x00 and 0x04. All five certificates are available on the Tesla Model 3 Key Fob.
You can see examples of the [certificates section](#certificates) at the
end of this document.

Each certificate is larger than 256 bytes, so support for
extended APDUs is required to get the entire certificate.
Because support for chaining does not appear to be
implemented and not all readers support extended APDUs, the
full certificate may be impossible to extract on hardware
that doesn't support extended APDUs.

The `CertID` is a value between 0 and 4. Values 0-3 presumably
refer to `KeyID`s 0-3. Typically only KeyID 0 has a certificate.

CertID | TKC (`teslaLogic002`) | TM3KF (`teslaLogic005`)
------:|-----------------------|------------
  0x00 | KeyID 0 Cert          | KeyID 0 Cert
  0x01 | *Err 6F17*            | KeyID 1 Cert
  0x02 | *Err 6F17*            | KeyID 2 Cert
  0x03 | *Err 6F17*            | Fob Issuing CA
  0x04 | root.tesla.com CA     | Fob Command CA
≥ 0x05 | *Err 6B00*            | *Err*

On the TKC, `CertID` 4 is some sort of root certificate, but
doesn't appear to be the certificate that signed
`KeyID` 0. You can see it and an example of a non-root
certificate at the end of this document.

On the TKC, The first two bytes of the returned result represent the
length of the certificate in bytes. There is no leading length word for the 
certificates on the TM3KF.

### INS 0x07: Get Versions

Field | Value | Notes
------|-------|---------------
CLA   | 0x80  | 
INS   | 0x07  | 
P1    | 0x00  | 
P2    | 0x00  |
Lc    | —     |
Data  | —     |
Le    | 0x00  |

Appears to return the version numbers for the `teslaLogic` and `teslaStore`, along
with an additional version to an unknown component.

**This command is not currently supported on the TPK.**

The versions are encoded as three 16-bit big-endian numbers. The first
corresponds with `teslaLogic` and the second corresponds with `teslaStore`. It
is unclear what the third corresponds with.

Order | TKC | TM3KF
------|-----|------------------
1st | 0x0002 | 0x0005
2nd | 0x0002 | 0x0003
3nd | 0x0002 | 0x0003

### INS 0x11: Authentication Challenge

Field | Value | Notes
------|-------|---------------
CLA   | 0x80  | 
INS   | 0x11  | 
P1    | 0x00-0x03 | Key Identifier 
P2    | 0x00  |
Lc    | 0x51  | 71 bytes in data field
Data  | 0x04 ... | 65-byte NIST P.256 Vehicle Public Key
&nbsp;| ...   | 16-byte Challenge
Le    | 0x00  |

This is the command the vehicle uses to authenticate the TKC.
The returned value is a 16-byte response.

The KeyID is a number between 0 and 3. Only 0x00 appears to be
currently used by the vehicle. It is assumed that this field
corresponds to the KeyID parameter from INS 0x04.

#### Algorithm

The 16-byte response is calculated as follows:

1. Calculate the ECDH shared secret.
2. Calculate the SHA-1 hash of the X parameter of the shared secret.
3. Truncate the SHA-1 hash to the most significant 128 bits. The result is `KEY`.
4. **OPTIONAL**: (TM3KF and TPK ONLY) The card writes over the first 4
   bytes of the challenge with random values.
   * `CHAL[0..3] = RANDDATA(4)` 
5. Perform a single block encrypt operation on the provided 16-byte challenge.
   * `RESP = AES_ENC(KEY,CHAL)`

To verify the credential, the car does the following:

1. Calculate the ECDH shared secret.
2. Calculate the SHA-1 hash of the X parameter of the shared secret.
3. Truncate the SHA-1 hash to the most significant 128 bits. The result is `KEY`.
4. Send a random challenge to the credential.
   * `CHAL[0..15] = RANDDATA(16)` 
5. Decrypt the response (`RESP`) when received.
   * `CHECK = AES__DEC(KEY,RESP)`
6. If the last 12 bytes of the challenge match the last 12 bytes of
   the decrypted response, the credential is authentic.
   * `ASSERT CHAL[4..15] == CHECK[4..15]`

### INS 0x14: Get Form Factor

Field | Value | Notes
------|-------|---------------
CLA   | 0x80  | 
INS   | 0x14  | 
P1    | 0x00  | 
P2    | 0x00  |
Lc    | —     |
Data  | —     |
Le    | 0x00  |

This command returns a two-byte form factor identifier.
This command is always used by the vehicle after the
authentication challenge response.

The value doesn't appear to currently change the behavior
of the vehicle UI.

Description          | Return Value
---------------------|---------------------
Tesla Key Card (TKC) | `0x0001`
Tesla Key Fob (TM3KF)| `0x0022`
Tesla Phone Key (TPK)| `0x0031`

### INS 0x1B: Set Vehicle Info

Field | Value | Notes
------|-------|---------------
CLA   | 0x80  | 
INS   | 0x1B  | 
P1    | 0x00  | 
P2    | 0x00  |
Lc    | 0x15  | 4 byte header + 17 byte VIN
Data  | 0x2a130a11 | ASN.1 Header
&nbsp;| ...   | 17-digit VIN (ASCII)
Le    | —     | No response data expected

This command is issued to the TPK after the normal pairing exchange.
It is only issued at pairing. The exact purpose of this command is unknown,
but the data sent from the vehicle is TLV-encoded (ASN.1?) and includes
the 17-digit VIN:

```
2a130a11 + <ASCII-ENCODED-VIN>

[2a] {
   [0a] <ASCII-ENCODED-VIN>
}
```

It seems that the TPK does not actually parse this data as ASN.1: it simply ignores
the first 3 bytes and assumes the fourth byte is the VIN length
and that the VIN starts at byte 5. That behavior may change, though.

## Other Unknown Commands

Through probing, we know the `teslaLogic` AID also supports
the following APDU instructions, but we don't yet know what they do:

* INS 0x00 -> Error 0x6f05
* INS 0x01 -> Success? 0x9000 (Takes a long time to complete, possibly a self-test)
* INS 0x02 -> Error 0x6f12
* INS 0x03 -> Error 0x6f12
* INS 0x05 -> Error 0x6f16
* INS 0x08 -> Success? 0x9000
* INS 0x12 -> Success? 0x9000
* INS 0x13 -> Error 0x6f1b
* INS 0x15 -> Error 0x6f1d
* INS 0xA4 -> Success? 0x9000 (Tesla Phone Key Only)

None of the above commands (except INS 0xA4) are implemented on the TPK.

## Authentication Process for TKC

1. Vehicle attempts to select AID `f465736c614c6f676963`
   * TKC indicates no such AID.
2. Vehicle attempts to select AID `7465736c614c6f676963`, which succeeds.
   * TKC indicates success
3. ***NEW:*** Vehicle requests the public key of the TKC using INS 4 APDU.
   * TKC responds with its public key.
4. Vehicle sends INS 0x11 APDU with vehicle public key and a random challenge.
   * TKC verifies public key is on curve P-256
   * TKC calculates and returns the response to the challenge
5. Vehicle sends INS 0x14 APDU.
   * TKC responds with the value 0x0001 and a successful response code.
6. Vehicle repeatedly sends junk APDUs to determine when TKC is removed.

## Pairing Process

1. Steps 1-5 of the authentication process are performed, presumably to ensure
   the presented card isn't one that is already known.
2. Vehicle sends INS 0x11 APDU with vehicle public key and challenge of all zeros.
   * TKC verifies public key is on curve P-256
   * TKC calculates and returns the response to the challenge
3. Vehicle repeatedly sends junk APDUs to determine when TKC is removed.

## Tesla Model 3 Key Fob Differences

The Tesla Key Fob also has an NFC interface. It kinda seems to
implement the same protocol, but there are some differences:

* INS 0x11 (Authenticate) doesn't seem to work the same way:
  the 16-byte response is different each time, due to it replacing
  the first four bytes of the challenge with random "salt".
  The documentation for INS 0x11 has been updated to reflect the changes.
* Three of the four ECDH keys has a certificate. The CN of each
  of these certificates has a suffix indcating which KeyID they are
  associated with: `- 0`/`- 1`/`- 2`.
* CertID3 seems to contain an intermediate CA: "Fob Issuing CA". It
  is independent of KeyID3, which seems to not have an associated certificate.
  The "Fob Issuing CA" is the issuing CA for CertID0, CertID1, and CertID2.
* CertID4 has a different intermediate CA: "Fob Command CA". It is unclear what this certificate is for.
* Both CertID3 and CertID4 are signed by a "Fob Root CA", which does not appear to be availble.
* The certificates don't appear to be prefixed with a length, so heuristics will be required to
  read certificates in an automated way.
* INS 0x07 returns `0005 0003 0003` instead of `0002 0002 0002` like on the TKC. This makes it seem likely that this command returns version information (TM3KF:`teslaLogic005/teslaStore003` vs TKC:`teslaLogic002/teslaStore002`).
* INS 0x14 (Get Form Factor) returns `0022` instead of `0001`.
* The `80ca2f00` trick yields the following AIDs:
   * `A000000151000000` -> Global Platform ISD
   * `5465736C61444150` -> `TeslaDAP`
   * `7465736C6153746F7265303033` -> `teslaStore003`
   * `7465736C614C6F676963303035` -> `teslaLogic005`

The pairing process, despite the additional BTLE pairing, is exactly the same.

## Tesla Phone Key Differences

The latest version of the Tesla App on Android provides a
NFC key interface (Tesla Phone Key, or TPK), allowing you
to use the phone just like a TKC. Pairing works the exact
same way. The protocol implemented by the TPK is a limited
subset of what is available on a TKC or TM3KF. Here are the
differences:

* INS (Authenticate) 0x11 includes random card-generated salt,
  just like the TM3KF.
* Has only one ECDH key instead of four like the TKC and TM3KF.
* There are no certificates.
* The command INS 0x14 returns `0x0031` instead of `0x0001` or `0x0022`.
* It includes a new command, INS 0x1B, which the vehicle uses
  to tell the phone its VIN. This command seems to only be sent to
  the TPK and not the TKC or TM3KF, so it may be introspecting into the UID to
  gate the issuing of this command.
* You must be logged into a Tesla account on the phone in order to use the
  NFC TPK functionality, so you can't use this feature to give
  someone access to your car unless they have a Tesla account.
  It is unclear what happens if you try to pair a phone with a vehicle
  on a different account. Command INS 0x1B implies that the phone
  might take notice and behave differently.
* If you explicltly sign out of the app and then sign back in, the public
  key is preserved—so NFC key access to any vehicles that was lost when
  you signed out should be restored when you sign back in. It is
  unclear if this is true if you sign in with a different account,
  but seems likely. In other words, *signing out doesn't wipe the
  NFC credential*.
* The vehicle was able to determine the name of the phone for its
  internal UI after pairing. This seems to imply either an in-band
  interaction that I missed or an out-of-band (BLE?) interaction.
  The vehicle seems to remember this sort of metadata even for
  deleted cards.

## Certificates

Tesla Key Cards/Fobs have X.509 certificates which can be retrieved via
INS 0x06. Below are real examples of the certificates that were extracted from a TKC
and a TM3KF:

### Card Certificate

```
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: 72515 (0x11b43)
    Signature Algorithm: ecdsa-with-SHA256
        Issuer: CN=Key Card CA, O=Selp
        Validity
            Not Before: Oct 11 16:48:35 2017 GMT
            Not After : Oct 11 16:48:35 2117 GMT
        Subject: CN=7179009621072316, OU=KEY CARD, O=Selp
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (256 bit)
                pub: 
                    04:68:0e:ad:81:7c:38:77:b7:ec:be:1e:0b:52:fb:
                    8c:93:17:92:37:7e:e9:57:57:c4:f8:6a:e7:55:ae:
                    cb:d6:28:66:9a:4a:2a:ac:97:7e:e2:29:e8:62:c3:
                    a3:c5:1b:3f:ec:18:90:3f:7b:e0:47:00:be:0c:e0:
                    6d:ae:aa:08:e4
                ASN1 OID: prime256v1
                NIST CURVE: P-256
        X509v3 extensions:
            X509v3 Basic Constraints: critical
                CA:FALSE
            X509v3 Key Usage: critical
                Digital Signature, Non Repudiation, Key Encipherment
            X509v3 Subject Key Identifier: 
                05:FE:FD:EB:27:A6:2C:C3:70:C4:08:9C:5F:31:1F:B1:43:D7:4D:55
            X509v3 Authority Key Identifier: 
                keyid:30:A1:05:FA:1D:23:E2:80:F3:73:4F:7A:E6:E1:7C:37:5D:01:F9:B2

            X509v3 Extended Key Usage: 
                TLS Web Client Authentication
    Signature Algorithm: ecdsa-with-SHA256
         30:45:02:20:15:74:40:b6:d4:05:92:d1:53:33:96:d4:cd:5e:
         97:a4:0f:00:4c:52:8e:a0:93:3a:a3:ed:50:fd:88:9f:8e:38:
         02:21:00:dd:87:44:45:94:36:53:fe:b9:ae:84:aa:eb:94:22:
         70:d9:a6:97:fc:69:53:91:86:9e:4f:3a:6e:8f:61:1f:21
-----BEGIN CERTIFICATE-----
MIIByjCCAXCgAwIBAgIDARtDMAoGCCqGSM49BAMCMCUxFDASBgNVBAMMC0tleSBD
YXJkIENBMQ0wCwYDVQQKDARTZWxwMCAXDTE3MTAxMTE2NDgzNVoYDzIxMTcxMDEx
MTY0ODM1WjA9MRkwFwYDVQQDDBA3MTc5MDA5NjIxMDcyMzE2MREwDwYDVQQLDAhL
RVkgQ0FSRDENMAsGA1UECgwEU2VscDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IA
BGgOrYF8OHe37L4eC1L7jJMXkjd+6VdXxPhq51Wuy9YoZppKKqyXfuIp6GLDo8Ub
P+wYkD974EcAvgzgba6qCOSjdTBzMAwGA1UdEwEB/wQCMAAwDgYDVR0PAQH/BAQD
AgXgMB0GA1UdDgQWBBQF/v3rJ6Ysw3DECJxfMR+xQ9dNVTAfBgNVHSMEGDAWgBQw
oQX6HSPigPNzT3rm4Xw3XQH5sjATBgNVHSUEDDAKBggrBgEFBQcDAjAKBggqhkjO
PQQDAgNIADBFAiAVdEC21AWS0VMzltTNXpekDwBMUo6gkzqj7VD9iJ+OOAIhAN2H
REWUNlP+ua6EquuUInDZppf8aVORhp5POm6PYR8h
-----END CERTIFICATE-----
```

Observations:

* Certificate serial number appears sequential. The other card in
  the same two-card pack had a certificate serial number of 72513.
* Tesla is not mentioned anywhere in the subject or issuer.
  Instead, the organization is listed as "Selp"; which likely
  referrs to the French smart-card/identity solution company
  [Selp](https://www.selp.fr/en/private-identity/).
* The CN of the subject is a 16-digit decimal number. Unlike the
  certificate serial number, this number appears to be randomly
  assigned.
* The certificate expires a hundred years after its issue date,
  which isn't necessarilly bad, but [there is a way to indicate
  that a certificate should not have an enforcable expiration
  date](https://tools.ietf.org/html/rfc5280#section-4.1.2.5), and
  using an expiration date of "today + 100years" isn't it.
  But really I'm just splitting hairs here.
* The key usage parameters are quite broad. It isn't clear how the
  current implementation of a TKC could actually perform any
  of them. I assume these fields are simply a misconfiguration.
* The CA that signed this certificate (Issuer `CN=Key Card CA, O=Selp`) is
  not the root certificate that is present on the card (see below).
  Because we don't have a copy of the the `CN=Key Card CA, O=Selp`
  certificate, we have no way to determine if the certificate is
  actually valid, which seems unfortunate.
  
### Card Root Certificate

```
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: 2 (0x2)
    Signature Algorithm: ecdsa-with-SHA256
        Issuer: CN=root.tesla.com
        Validity
            Not Before: Jan  9 15:37:10 2017 GMT
            Not After : Jan  9 15:37:10 2018 GMT
        Subject: CN=root.tesla.com
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (256 bit)
                pub: 
                    04:ba:05:a4:c5:58:5d:5f:f8:52:0f:1c:3b:c8:b3:
                    02:48:04:56:7c:b0:d2:fe:a3:0d:23:62:51:5e:6a:
                    ad:51:86:3c:3c:b6:24:49:3e:c6:f6:b9:e2:00:43:
                    11:83:eb:bd:64:bb:95:f3:82:f5:14:b7:ff:3b:d7:
                    e5:20:5c:7f:60
                ASN1 OID: prime256v1
                NIST CURVE: P-256
        X509v3 extensions:
            X509v3 Basic Constraints: 
                CA:TRUE
    Signature Algorithm: ecdsa-with-SHA256
         30:46:02:21:00:9c:b1:0d:dd:a8:30:b8:7e:ca:aa:d1:39:e3:
         98:76:6c:6e:23:44:78:23:35:be:bb:76:d8:26:7b:d8:1a:05:
         26:02:21:00:8d:6e:00:05:fc:4e:46:3b:80:43:9c:fb:f5:c2:
         18:3c:58:ce:e3:86:7d:89:69:a7:71:62:4b:5a:99:cb:1f:53
-----BEGIN CERTIFICATE-----
MIIBMTCB16ADAgECAgECMAoGCCqGSM49BAMCMBkxFzAVBgNVBAMTDnJvb3QudGVz
bGEuY29tMB4XDTE3MDEwOTE1MzcxMFoXDTE4MDEwOTE1MzcxMFowGTEXMBUGA1UE
AxMOcm9vdC50ZXNsYS5jb20wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAS6BaTF
WF1f+FIPHDvIswJIBFZ8sNL+ow0jYlFeaq1Rhjw8tiRJPsb2ueIAQxGD671ku5Xz
gvUUt/871+UgXH9goxAwDjAMBgNVHRMEBTADAQH/MAoGCCqGSM49BAMCA0kAMEYC
IQCcsQ3dqDC4fsqq0TnjmHZsbiNEeCM1vrt22CZ72BoFJgIhAI1uAAX8TkY7gEOc
+/XCGDxYzuOGfYlpp3FiS1qZyx9T
-----END CERTIFICATE-----
```

Observations:

* This is not the root certificate that signed the card certificate.
* This certificate has expired.
* Missing "X509v3 Subject Key Identifier" field, which seems
  unfortunate.
* Feels bogus, like this was a test certificate that someone forgot
  to replace with the real root certificate. Not sure what the story
  is here. Maybe this is the legit root, and `CN=Key Card CA, O=Selp`
  is just an intermediate CA?

### Fob Certificate

```
Certificate:
    Data:
        Version: 1 (0x0)
        Serial Number: 81646 (0x13eee)
    Signature Algorithm: ecdsa-with-SHA256
        Issuer: C=US, O=Tesla, CN=Fob Issuing CA 001
        Validity
            Not Before: Sep 28 02:59:05 2019 GMT
            Not After : Sep 25 02:59:05 2029 GMT
        Subject: C=US, ST=California, O=Tesla, OU=Model3 Fobs, CN=JBL19268KBH53F - 0
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (256 bit)
                pub: 
                    04:01:6d:fd:d2:de:7f:1f:96:91:f6:15:19:16:62:
                    1b:be:98:a2:a7:40:66:fc:71:91:9d:99:68:b6:03:
                    51:a5:68:ba:5f:13:9e:30:3d:e3:ff:bb:c4:10:1e:
                    dd:9d:bf:3a:47:cd:33:fc:47:1f:04:6b:ac:20:db:
                    f1:3e:54:11:58
                ASN1 OID: prime256v1
                NIST CURVE: P-256
    Signature Algorithm: ecdsa-with-SHA256
         30:45:02:21:00:97:f7:ea:b4:6d:9d:f4:09:1d:35:b1:25:32:
         6d:53:0b:8b:c4:07:c9:7c:b4:23:7e:52:ff:db:0b:e3:dd:06:
         5c:02:20:4b:bc:6f:ac:20:12:12:19:59:66:c6:dc:a8:3b:ad:
         f7:1a:02:ec:db:74:e1:b1:e4:6c:47:9f:be:a7:15:bb:41
-----BEGIN CERTIFICATE-----
MIIBiTCCAS8CAwE+7jAKBggqhkjOPQQDAjA6MQswCQYDVQQGEwJVUzEOMAwGA1UE
CgwFVGVzbGExGzAZBgNVBAMMEkZvYiBJc3N1aW5nIENBIDAwMTAeFw0xOTA5Mjgw
MjU5MDVaFw0yOTA5MjUwMjU5MDVaMGUxCzAJBgNVBAYTAlVTMRMwEQYDVQQIDApD
YWxpZm9ybmlhMQ4wDAYDVQQKDAVUZXNsYTEUMBIGA1UECwwLTW9kZWwzIEZvYnMx
GzAZBgNVBAMMEkpCTDE5MjY4S0JINTNGIC0gMDBZMBMGByqGSM49AgEGCCqGSM49
AwEHA0IABAFt/dLefx+WkfYVGRZiG76YoqdAZvxxkZ2ZaLYDUaVoul8TnjA94/+7
xBAe3Z2/OkfNM/xHHwRrrCDb8T5UEVgwCgYIKoZIzj0EAwIDSAAwRQIhAJf36rRt
nfQJHTWxJTJtUwuLxAfJfLQjflL/2wvj3QZcAiBLvG+sIBISGVlmxtyoO633GgLs
23ThseRsR5++pxW7QQ==
-----END CERTIFICATE-----
```

Observations:

* Subject and issuer clearly indicate Tesla, not Selp.
* Subject and issuer have fields reversed compared to card certificates.
* Missing both "Subject Key Identifier" and "Authority Key Idenifier".
* Fob is clearly indicated as being intended for Model 3.
* Expiration date is only 10 years in the future.

## Fob Issuing CA

```
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: 4096 (0x1000)
    Signature Algorithm: ecdsa-with-SHA256
        Issuer: C=US, ST=California, O=Tesla, OU=Tesla Motors, CN=Fob Root CA
        Validity
            Not Before: Jul 13 01:26:07 2018 GMT
            Not After : Jan 24 01:26:07 2039 GMT
        Subject: C=US, O=Tesla, CN=Fob Issuing CA 001
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (256 bit)
                pub: 
                    04:26:ed:2b:8c:57:89:71:03:58:94:fe:6a:70:84:
                    99:a5:e6:29:47:a3:8e:7b:e2:8b:18:d3:63:27:9e:
                    39:6f:d6:3a:f5:b1:b3:42:af:df:97:07:42:99:29:
                    3c:8a:39:8a:8e:14:f1:44:79:91:74:8e:95:ad:20:
                    71:67:07:59:f7
                ASN1 OID: prime256v1
                NIST CURVE: P-256
        X509v3 extensions:
            X509v3 Subject Key Identifier: 
                8B:BC:81:95:D0:5F:9F:5C:EF:C0:61:ED:A1:DB:A1:49:86:0B:E6:72
            X509v3 Authority Key Identifier: 
                keyid:FB:27:C7:61:4A:3B:19:CC:47:EE:D4:95:80:99:28:8A:2E:E8:58:8E

            X509v3 Basic Constraints: critical
                CA:TRUE, pathlen:0
            X509v3 Key Usage: critical
                Digital Signature, Certificate Sign, CRL Sign
    Signature Algorithm: ecdsa-with-SHA256
         30:81:87:02:42:01:ee:21:17:06:81:25:b8:20:e5:3f:4e:4b:
         a7:29:be:54:d1:c8:6e:aa:75:1a:7d:18:67:f5:9a:6b:ef:9d:
         db:73:2f:00:d5:da:a2:93:6b:fb:1d:ae:c0:d5:1a:5c:11:5f:
         ef:ac:4d:40:85:a4:35:65:47:ce:8e:72:8b:f3:29:90:ab:02:
         41:6d:79:cc:21:27:6a:8c:4c:18:ed:b0:ae:71:6b:eb:60:ce:
         5e:1e:4b:85:25:72:e4:1f:de:65:83:bf:38:de:62:72:9b:1d:
         e7:5f:94:da:da:ec:20:34:e2:b1:f2:09:cc:14:ba:aa:0b:ca:
         ae:7e:7d:b8:d3:0d:11:03:e8:6c:7e:00
-----BEGIN CERTIFICATE-----
MIICMzCCAZWgAwIBAgICEAAwCgYIKoZIzj0EAwIwXzELMAkGA1UEBhMCVVMxEzAR
BgNVBAgMCkNhbGlmb3JuaWExDjAMBgNVBAoMBVRlc2xhMRUwEwYDVQQLDAxUZXNs
YSBNb3RvcnMxFDASBgNVBAMMC0ZvYiBSb290IENBMB4XDTE4MDcxMzAxMjYwN1oX
DTM5MDEyNDAxMjYwN1owOjELMAkGA1UEBhMCVVMxDjAMBgNVBAoMBVRlc2xhMRsw
GQYDVQQDDBJGb2IgSXNzdWluZyBDQSAwMDEwWTATBgcqhkjOPQIBBggqhkjOPQMB
BwNCAAQm7SuMV4lxA1iU/mpwhJml5ilHo4574osY02Mnnjlv1jr1sbNCr9+XB0KZ
KTyKOYqOFPFEeZF0jpWtIHFnB1n3o2YwZDAdBgNVHQ4EFgQUi7yBldBfn1zvwGHt
oduhSYYL5nIwHwYDVR0jBBgwFoAU+yfHYUo7GcxH7tSVgJkoii7oWI4wEgYDVR0T
AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwCgYIKoZIzj0EAwIDgYsAMIGH
AkIB7iEXBoEluCDlP05Lpym+VNHIbqp1Gn0YZ/Waa++d23MvANXaopNr+x2uwNUa
XBFf76xNQIWkNWVHzo5yi/MpkKsCQW15zCEnaoxMGO2wrnFr62DOXh5LhSVy5B/e
ZYO/ON5icpsd51+U2trsIDTisfIJzBS6qgvKrn59uNMNEQPobH4A
-----END CERTIFICATE-----
```

## Fob Command CA

```
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: 4098 (0x1002)
    Signature Algorithm: ecdsa-with-SHA256
        Issuer: C=US, ST=California, O=Tesla, OU=Tesla Motors, CN=Fob Root CA
        Validity
            Not Before: Sep 22 00:06:06 2018 GMT
            Not After : Apr  5 00:06:06 2039 GMT
        Subject: C=US, O=Tesla, CN=Fob Command CA
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (521 bit)
                pub: 
                    04:01:50:79:90:b9:2a:10:00:1b:c9:07:bd:f2:3c:
                    05:a2:47:d5:2c:ea:e7:3e:d4:ec:20:54:18:90:fd:
                    04:af:71:21:21:40:5b:0c:8e:46:19:3c:9b:63:89:
                    9e:25:25:39:fb:b6:65:8f:a3:3a:d8:75:f5:d8:10:
                    d3:17:04:ef:05:74:ce:01:f8:9a:8d:9e:c1:57:17:
                    b3:15:be:4d:ca:61:a6:15:49:71:8e:9a:67:2d:98:
                    03:f1:5e:ea:d3:b6:0a:3d:d2:c3:1a:4d:c8:41:b6:
                    3f:95:e5:61:93:ba:38:e4:99:3d:9b:7a:4f:d5:63:
                    2b:4c:e9:0b:3c:cd:f9:ec:ce:5b:7f:ca:e9
                ASN1 OID: secp521r1
                NIST CURVE: P-521
        X509v3 extensions:
            X509v3 Subject Key Identifier: 
                38:2C:EC:B4:E6:2C:B8:F6:6E:74:35:56:75:55:05:A0:B4:2E:92:F2
            X509v3 Authority Key Identifier: 
                keyid:FB:27:C7:61:4A:3B:19:CC:47:EE:D4:95:80:99:28:8A:2E:E8:58:8E

            X509v3 Basic Constraints: critical
                CA:TRUE, pathlen:0
            X509v3 Key Usage: critical
                Digital Signature, Certificate Sign, CRL Sign
    Signature Algorithm: ecdsa-with-SHA256
         30:81:88:02:42:01:fb:04:7c:ae:bd:f7:ea:0b:ee:d0:7b:45:
         54:ab:f6:d7:9e:89:21:32:cf:7a:d5:f5:57:4f:b7:aa:91:b6:
         f7:04:e6:99:ce:a2:5e:01:10:71:e8:a3:e4:07:dd:67:54:5f:
         48:3f:90:62:73:c0:25:0c:a1:2a:d8:a0:b3:bf:ee:f4:c7:02:
         42:01:f9:f2:c7:1c:90:a5:0f:9e:db:73:e2:1d:cb:f0:82:89:
         4d:2e:6a:03:0e:09:bc:3a:c1:20:75:5b:e7:00:e8:1a:3f:13:
         91:dc:d0:73:66:3b:9d:df:28:ff:47:a9:06:02:ab:e2:16:79:
         82:4a:c5:1a:cc:e4:61:48:64:fb:56:3a:8c
-----BEGIN CERTIFICATE-----
MIICczCCAdSgAwIBAgICEAIwCgYIKoZIzj0EAwIwXzELMAkGA1UEBhMCVVMxEzAR
BgNVBAgMCkNhbGlmb3JuaWExDjAMBgNVBAoMBVRlc2xhMRUwEwYDVQQLDAxUZXNs
YSBNb3RvcnMxFDASBgNVBAMMC0ZvYiBSb290IENBMB4XDTE4MDkyMjAwMDYwNloX
DTM5MDQwNTAwMDYwNlowNjELMAkGA1UEBhMCVVMxDjAMBgNVBAoMBVRlc2xhMRcw
FQYDVQQDDA5Gb2IgQ29tbWFuZCBDQTCBmzAQBgcqhkjOPQIBBgUrgQQAIwOBhgAE
AVB5kLkqEAAbyQe98jwFokfVLOrnPtTsIFQYkP0Er3EhIUBbDI5GGTybY4meJSU5
+7Zlj6M62HX12BDTFwTvBXTOAfiajZ7BVxezFb5NymGmFUlxjppnLZgD8V7q07YK
PdLDGk3IQbY/leVhk7o45Jk9m3pP1WMrTOkLPM357M5bf8rpo2YwZDAdBgNVHQ4E
FgQUOCzstOYsuPZudDVWdVUFoLQukvIwHwYDVR0jBBgwFoAU+yfHYUo7GcxH7tSV
gJkoii7oWI4wEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwCgYI
KoZIzj0EAwIDgYwAMIGIAkIB+wR8rr336gvu0HtFVKv2156JITLPetX1V0+3qpG2
9wTmmc6iXgEQceij5AfdZ1RfSD+QYnPAJQyhKtigs7/u9McCQgH58scckKUPnttz
4h3L8IKJTS5qAw4JvDrBIHVb5wDoGj8TkdzQc2Y7nd8o/0epBgKr4hZ5gkrFGszk
YUhk+1Y6jA==
-----END CERTIFICATE-----
```

## Acknowledgements and Thanks

 * [`u/rad_example`](https://www.reddit.com/user/rad_example/), for [pointing out](https://www.reddit.com/r/teslamotors/comments/drksso/how_tesla_key_cards_actually_work/f7bcbpv/) that `O=Selp` likely referrs to [selp.fr](https://www.selp.fr/en/private-identity/).
 * [Martin Paljack](https://github.com/martinpaljak/) for both [GlobalPlatformPro](https://github.com/martinpaljak/GlobalPlatformPro) and [ant-javacard](https://github.com/martinpaljak/ant-javacard), both of which I find indespensible.
 * Jonathan Westhues, for creating the [Proxmark3](http://www.proxmark.org/), along with everyone else maintaining [the software](https://github.com/Proxmark/proxmark3) for this indespensible tool.
 * [Tesla](https://tesla.com/), for making such a great vehicle
 