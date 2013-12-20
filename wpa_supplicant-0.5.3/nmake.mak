# Makefile for Microsoft nmake to build wpa_supplicant

# This can be run in Visual Studio 2005 Command Prompt

# Note: Make sure that cl.exe is configured to include Platform SDK
# include and lib directories (vsvars32.bat)

all: wpa_supplicant.exe wpa_cli.exe wpa_passphrase.exe wpasvc.exe

# Root directory for WinPcap developer's pack
# (http://www.winpcap.org/install/bin/WpdPack_3_1.zip)
WINPCAPDIR=C:\dev\WpdPack

# Root directory for OpenSSL
# (http://www.openssl.org/source/openssl-0.9.8a.tar.gz)
# Build and installed following instructions in INSTALL.W32
# Note: If EAP-FAST is included in the build, OpenSSL needs to be patched to
# support it (openssl-tls-extensions.patch)
OPENSSLDIR=C:\dev\openssl

CC = cl
OBJDIR = objs

CFLAGS = /DCONFIG_NATIVE_WINDOWS
CFLAGS = $(CFLAGS) /DCONFIG_NDIS_EVENTS_INTEGRATED
CFLAGS = $(CFLAGS) /DCONFIG_ANSI_C_EXTRA
CFLAGS = $(CFLAGS) /DCONFIG_WINPCAP
CFLAGS = $(CFLAGS) /DIEEE8021X_EAPOL
CFLAGS = $(CFLAGS) /DEAP_TLS_FUNCS
CFLAGS = $(CFLAGS) /DPKCS12_FUNCS
CFLAGS = $(CFLAGS) /DEAP_MD5
CFLAGS = $(CFLAGS) /DEAP_TLS
CFLAGS = $(CFLAGS) /DEAP_MSCHAPv2
CFLAGS = $(CFLAGS) /DEAP_PEAP
CFLAGS = $(CFLAGS) /DEAP_TTLS
CFLAGS = $(CFLAGS) /DEAP_GTC
CFLAGS = $(CFLAGS) /DEAP_OTP
CFLAGS = $(CFLAGS) /DEAP_SIM
CFLAGS = $(CFLAGS) /DEAP_LEAP
CFLAGS = $(CFLAGS) /DEAP_PSK
CFLAGS = $(CFLAGS) /DEAP_AKA
#CFLAGS = $(CFLAGS) /DEAP_FAST
CFLAGS = $(CFLAGS) /DEAP_PAX
CFLAGS = $(CFLAGS) /DPCSC_FUNCS
CFLAGS = $(CFLAGS) /DCONFIG_CTRL_IFACE
CFLAGS = $(CFLAGS) /DCONFIG_CTRL_IFACE_UDP
CFLAGS = $(CFLAGS) /DCONFIG_DRIVER_NDIS
CFLAGS = $(CFLAGS) /I..\hostapd /I.
CFLAGS = $(CFLAGS) /DWIN32
CFLAGS = $(CFLAGS) /Fo$(OBJDIR)\\ /c
CFLAGS = $(CFLAGS) /W3

#CFLAGS = $(CFLAGS) /WX

# VS 2005 complains about lot of deprecated string functions; let's ignore them
# at least for now since snprintf and strncpy can be used in a safe way
CFLAGS = $(CFLAGS) /D_CRT_SECURE_NO_DEPRECATE

OBJS = \
	$(OBJDIR)\os_win32.obj \
	$(OBJDIR)\eloop_win.obj \
	$(OBJDIR)\sha1.obj \
	$(OBJDIR)\md5.obj \
	$(OBJDIR)\rc4.obj \
	$(OBJDIR)\aes_wrap.obj \
	$(OBJDIR)\common.obj \
	$(OBJDIR)\wpa_supplicant.obj \
	$(OBJDIR)\wpa.obj \
	$(OBJDIR)\preauth.obj \
	$(OBJDIR)\pmksa_cache.obj \
	$(OBJDIR)\eapol_sm.obj \
	$(OBJDIR)\eap.obj \
	$(OBJDIR)\eap_methods.obj \
	$(OBJDIR)\eap_tlv.obj \
	$(OBJDIR)\eap_md5.obj \
	$(OBJDIR)\eap_tls.obj \
	$(OBJDIR)\eap_tls_common.obj \
	$(OBJDIR)\eap_mschapv2.obj \
	$(OBJDIR)\eap_peap.obj \
	$(OBJDIR)\eap_ttls.obj \
	$(OBJDIR)\eap_gtc.obj \
	$(OBJDIR)\eap_otp.obj \
	$(OBJDIR)\eap_leap.obj \
	$(OBJDIR)\eap_sim.obj \
	$(OBJDIR)\eap_sim_common.obj \
	$(OBJDIR)\eap_aka.obj \
	$(OBJDIR)\eap_pax.obj \
	$(OBJDIR)\eap_pax_common.obj \
	$(OBJDIR)\eap_psk.obj \
	$(OBJDIR)\eap_psk_common.obj \
	$(OBJDIR)\ctrl_iface.obj \
	$(OBJDIR)\ctrl_iface_udp.obj \
	$(OBJDIR)\driver_ndis.obj \
	$(OBJDIR)\driver_ndis_.obj \
	$(OBJDIR)\events.obj \
	$(OBJDIR)\config.obj \
	$(OBJDIR)\l2_packet_winpcap.obj \
	$(OBJDIR)\tls_openssl.obj \
	$(OBJDIR)\ms_funcs.obj \
	$(OBJDIR)\crypto.obj \
	$(OBJDIR)\pcsc_funcs.obj \
	$(OBJDIR)\ndis_events.obj

# OBJS = $(OBJS) $(OBJDIR)\eap_fast.obj

OBJS_t = $(OBJS) \
	$(OBJDIR)\eapol_test.obj \
	$(OBJDIR)\radius.obj \
	$(OBJDIR)\radius_client.obj \
	$(OBJDIR)\config_file.obj $(OBJDIR)\base64.obj

OBJS_t2 = $(OBJS) \
	$(OBJDIR)\preauth_test.obj \
	$(OBJDIR)\config_file.obj $(OBJDIR)\base64.obj

OBJS2 = $(OBJDIR)\drivers.obj \
	$(OBJDIR)\config_file.obj $(OBJDIR)\base64.obj \
	$(OBJS2) $(OBJDIR)\main.obj

OBJS3 = $(OBJDIR)\drivers.obj \
	$(OBJDIR)\config_winreg.obj \
	$(OBJS3) $(OBJDIR)\main_winsvc.obj

OBJS_c = \
	$(OBJDIR)\os_win32.obj \
	$(OBJDIR)\wpa_cli.obj \
	$(OBJDIR)\wpa_ctrl.obj \
	$(OBJDIR)\common.obj

OBJS_p = \
	$(OBJDIR)\os_win32.obj \
	$(OBJDIR)\common.obj \
	$(OBJDIR)\sha1.obj \
	$(OBJDIR)\md5.obj \
	$(OBJDIR)\crypto.obj \
	$(OBJDIR)\wpa_passphrase.obj

LIBS = wbemuuid.lib libcmt.lib kernel32.lib uuid.lib ole32.lib oleaut32.lib \
	ws2_32.lib Advapi32.lib Crypt32.lib Winscard.lib \
	Packet.lib wpcap.lib \
	libeay32.lib ssleay32.lib

CFLAGS = $(CFLAGS) /I"$(WINPCAPDIR)/Include" /I"$(OPENSSLDIR)\include"
LFLAGS = /libpath:"$(WINPCAPDIR)\Lib" /libpath:"$(OPENSSLDIR)\lib"

wpa_supplicant.exe: $(OBJDIR) $(OBJS) $(OBJS2)
	link.exe /out:wpa_supplicant.exe $(LFLAGS) $(OBJS) $(OBJS2) $(LIBS)

wpasvc.exe: $(OBJDIR) $(OBJS) $(OBJS3)
	link.exe /out:wpasvc.exe $(LFLAGS) $(OBJS) $(OBJS3) $(LIBS)

wpa_cli.exe: $(OBJDIR) $(OBJS_c)
	link.exe /out:wpa_cli.exe $(LFLAGS) $(OBJS_c) $(LIBS)

wpa_passphrase.exe: $(OBJDIR) $(OBJS_p)
	link.exe /out:wpa_passphrase.exe $(LFLAGS) $(OBJS_p) $(LIBS)

eapol_test.exe: $(OBJDIR) $(OBJS_t)
	link.exe /out:eapol_test.exe $(LFLAGS) $(OBJS_t) $(LIBS)

preauth_test.exe: $(OBJDIR) $(OBJS_t2)
	link.exe /out:preauth_test.exe $(LFLAGS) $(OBJS_t2) $(LIBS)


{..\hostapd}.c{$(OBJDIR)}.obj::
	$(CC) $(CFLAGS) $<

{.\}.c{$(OBJDIR)}.obj::
	$(CC) $(CFLAGS) $<

{.\}.cpp{$(OBJDIR)}.obj::
	$(CC) $(CFLAGS) $<

$(OBJDIR):
	if not exist "$(OBJDIR)" mkdir "$(OBJDIR)"

clean:
	erase $(OBJDIR)\*.obj wpa_supplicant.exe
