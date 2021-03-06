/*
 * WPA Supplicant / Configuration backend: Windows registry
 * Copyright (c) 2003-2006, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * This file implements a configuration backend for Windows registry.. All the
 * configuration information is stored in the registry and the format for
 * network configuration fields is same as described in the sample
 * configuration file, wpa_supplicant.conf.
 *
 * Configuration data is in HKEY_LOCAL_MACHINE\SOFTWARE\wpa_supplicant\configs
 * key. Each configuration profile has its own key under this. In terms of text
 * files, each profile would map to a separate text file with possibly multiple
 * networks. Under each profile, there is a networks key that lists all
 * networks as a subkey. Each network has set of values in the same way as
 * network block in the configuration file. In addition, blobs subkey has
 * possible blobs as values.
 *
 * HKEY_LOCAL_MACHINE\SOFTWARE\wpa_supplicant\configs\test\networks\0000
 *    ssid="example"
 *    key_mgmt=WPA-PSK
 */

#include "includes.h"

#include "common.h"
#include "wpa.h"
#include "wpa_supplicant.h"
#include "config.h"

#define KEY_ROOT HKEY_LOCAL_MACHINE
#define KEY_PREFIX "SOFTWARE\\wpa_supplicant"


static int wpa_config_read_blobs(struct wpa_config *config, HKEY hk)
{
	struct wpa_config_blob *blob;
	int errors = 0;
	HKEY bhk;
	LONG ret;
	DWORD i;

	ret = RegOpenKeyEx(hk, "blobs", 0, KEY_QUERY_VALUE, &bhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "Could not open wpa_supplicant config "
			   "blobs key");
		return 0; /* assume no blobs */
	}

	for (i = 0; ; i++) {
		char name[255], data[4096];
		DWORD namelen, datalen, type;

		namelen = sizeof(name);
		datalen = sizeof(data);
		ret = RegEnumValue(bhk, i, name, &namelen, NULL, &type,
				   data, &datalen);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegEnumValue failed: 0x%x",
				   (unsigned int) ret);
			break;
		}

		if (namelen >= sizeof(name))
			namelen = sizeof(name) - 1;
		name[namelen] = '\0';

		if (datalen >= sizeof(data))
			datalen = sizeof(data) - 1;
		data[datalen] = '\0';

		wpa_printf(MSG_MSGDUMP, "blob %d: field='%s' len %d",
			   (int) i, name, (int) datalen);

		blob = wpa_zalloc(sizeof(*blob));
		if (blob == NULL) {
			errors++;
			break;
		}
		blob->name = strdup(name);
		blob->data = malloc(datalen);
		if (blob->name == NULL || blob->data == NULL) {
			wpa_config_free_blob(blob);
			errors++;
			break;
		}
		memcpy(blob->data, data, datalen);
		blob->len = datalen;

		wpa_config_set_blob(config, blob);
	}

	RegCloseKey(bhk);

	return errors ? -1 : 0;
}


static int wpa_config_read_reg_dword(HKEY hk, const char *name, int *_val)
{
	DWORD val, buflen;
	LONG ret;

	buflen = sizeof(val);
	ret = RegQueryValueEx(hk, name, NULL, NULL, (LPBYTE) &val, &buflen);
	if (ret == ERROR_SUCCESS && buflen == sizeof(val)) {
		wpa_printf(MSG_DEBUG, "%s=%d", name, (int) val);
		*_val = val;
		return 0;
	}

	return -1;
}


static char * wpa_config_read_reg_string(HKEY hk, const char *name)
{
	DWORD buflen;
	LONG ret;
	char *val;

	buflen = 0;
	ret = RegQueryValueEx(hk, name, NULL, NULL, NULL, &buflen);
	if (ret != ERROR_SUCCESS)
		return NULL;
	val = malloc(buflen + 1);
	if (val == NULL)
		return NULL;
	val[buflen] = '\0';

	ret = RegQueryValueEx(hk, name, NULL, NULL, (LPBYTE) val, &buflen);
	if (ret != ERROR_SUCCESS) {
		free(val);
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "%s=%s", name, val);
	return val;
}


static int wpa_config_read_global(struct wpa_config *config, HKEY hk)
{
	int errors = 0;

	wpa_config_read_reg_dword(hk, "ap_scan", &config->ap_scan);
	wpa_config_read_reg_dword(hk, "fast_reauth", &config->fast_reauth);
	wpa_config_read_reg_dword(hk, "dot11RSNAConfigPMKLifetime",
				  &config->dot11RSNAConfigPMKLifetime);
	wpa_config_read_reg_dword(hk, "dot11RSNAConfigPMKReauthThreshold",
				  &config->dot11RSNAConfigPMKReauthThreshold);
	wpa_config_read_reg_dword(hk, "dot11RSNAConfigSATimeout",
				  &config->dot11RSNAConfigSATimeout);
	wpa_config_read_reg_dword(hk, "update_config", &config->update_config);

	if (wpa_config_read_reg_dword(hk, "eapol_version",
				      &config->eapol_version) == 0) {
		if (config->eapol_version < 1 ||
		    config->eapol_version > 2) {
			wpa_printf(MSG_ERROR, "Invalid EAPOL version (%d)",
				   config->eapol_version);
			errors++;
		}
	}

	config->ctrl_interface = wpa_config_read_reg_string(hk,
							    "ctrl_interface");

	return errors ? -1 : 0;
}


static struct wpa_ssid * wpa_config_read_network(HKEY hk, const char *netw,
						 int id)
{
	HKEY nhk;
	LONG ret;
	DWORD i;
	struct wpa_ssid *ssid;
	int errors = 0;

	ret = RegOpenKeyEx(hk, netw, 0, KEY_QUERY_VALUE, &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "Could not open wpa_supplicant config "
			   "network '%s'", netw);
		return NULL;
	}

	wpa_printf(MSG_MSGDUMP, "Start of a new network '%s'", netw);
	ssid = wpa_zalloc(sizeof(*ssid));
	if (ssid == NULL) {
		RegCloseKey(nhk);
		return NULL;
	}
	ssid->id = id;

	wpa_config_set_network_defaults(ssid);

	for (i = 0; ; i++) {
		TCHAR name[255], data[1024];
		DWORD namelen, datalen, type;

		namelen = 255;
		datalen = 1024;
		ret = RegEnumValue(nhk, i, name, &namelen, NULL, &type,
				   data, &datalen);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_ERROR, "RegEnumValue failed: 0x%x",
				   (unsigned int) ret);
			break;
		}

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = '\0';

		if (datalen >= 1024)
			datalen = 1024 - 1;
		data[datalen] = '\0';

		if (wpa_config_set(ssid, name, data, 0) < 0)
			errors++;
	}

	RegCloseKey(nhk);

	if (ssid->passphrase) {
		if (ssid->psk_set) {
			wpa_printf(MSG_ERROR, "Both PSK and passphrase "
				   "configured for network '%s'.", netw);
			errors++;
		}
		wpa_config_update_psk(ssid);
	}

	if ((ssid->key_mgmt & WPA_KEY_MGMT_PSK) && !ssid->psk_set) {
		wpa_printf(MSG_ERROR, "WPA-PSK accepted for key management, "
			   "but no PSK configured for network '%s'.", netw);
		errors++;
	}

	if ((ssid->group_cipher & WPA_CIPHER_CCMP) &&
	    !(ssid->pairwise_cipher & WPA_CIPHER_CCMP)) {
		/* Group cipher cannot be stronger than the pairwise cipher. */
		wpa_printf(MSG_DEBUG, "Removed CCMP from group cipher "
			   "list since it was not allowed for pairwise "
			   "cipher for network '%s'.", netw);
		ssid->group_cipher &= ~WPA_CIPHER_CCMP;
	}

	if (errors) {
		wpa_config_free_ssid(ssid);
		ssid = NULL;
	}

	return ssid;
}


static int wpa_config_read_networks(struct wpa_config *config, HKEY hk)
{
	HKEY nhk;
	struct wpa_ssid *ssid, *tail = NULL, *head = NULL;
	int errors = 0;
	LONG ret;
	DWORD i;

	ret = RegOpenKeyEx(hk, "networks", 0, KEY_ENUMERATE_SUB_KEYS, &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "Could not open wpa_supplicant networks "
			   "registry key");
		return -1;
	}

	for (i = 0; ; i++) {
		TCHAR name[255];
		DWORD namelen;

		namelen = 255;
		ret = RegEnumKeyEx(nhk, i, name, &namelen, NULL, NULL, NULL,
				   NULL);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegEnumKeyEx failed: 0x%x",
				   (unsigned int) ret);
			break;
		}

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = '\0';

		ssid = wpa_config_read_network(nhk, name, i);
		if (ssid == NULL) {
			wpa_printf(MSG_ERROR, "Failed to parse network "
				   "profile '%s'.", name);
			errors++;
			continue;
		}
		if (head == NULL) {
			head = tail = ssid;
		} else {
			tail->next = ssid;
			tail = ssid;
		}
		if (wpa_config_add_prio_network(config, ssid)) {
			wpa_printf(MSG_ERROR, "Failed to add network profile "
				   "'%s' to priority list.", name);
			errors++;
			continue;
		}
	}

	RegCloseKey(nhk);

	config->ssid = head;

	return errors ? -1 : 0;
}


struct wpa_config * wpa_config_read(const char *name)
{
	char buf[256];
	int errors = 0;
	struct wpa_config *config;
	struct wpa_ssid *ssid;
	int prio;
	HKEY hk;
	LONG ret;

	config = wpa_config_alloc_empty(NULL, NULL);
	if (config == NULL)
		return NULL;
	wpa_printf(MSG_DEBUG, "Reading configuration profile '%s'", name);

	snprintf(buf, sizeof(buf), KEY_PREFIX "\\configs\\%s", name);
	ret = RegOpenKeyEx(KEY_ROOT, buf, 0, KEY_QUERY_VALUE, &hk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "Could not open wpa_supplicant "
			   "configuration registry %s", buf);
		free(config);
		return NULL;
	}

	if (wpa_config_read_global(config, hk))
		errors++;

	if (wpa_config_read_networks(config, hk))
		errors++;

	if (wpa_config_read_blobs(config, hk))
		errors++;

	for (prio = 0; prio < config->num_prio; prio++) {
		ssid = config->pssid[prio];
		wpa_printf(MSG_DEBUG, "Priority group %d",
			   ssid->priority);
		while (ssid) {
			wpa_printf(MSG_DEBUG, "   id=%d ssid='%s'",
				   ssid->id,
				   wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
			ssid = ssid->pnext;
		}
	}

	RegCloseKey(hk);

	if (errors) {
		wpa_config_free(config);
		config = NULL;
	}

	return config;
}


static int wpa_config_write_reg_dword(HKEY hk, const char *name, int val,
				      int def)
{
	LONG ret;
	DWORD _val = val;

	if (val == def) {
		RegDeleteValue(hk, name);
		return 0;
	}

	ret = RegSetValueEx(hk, name, 0, REG_DWORD, (LPBYTE) &_val,
			    sizeof(_val));
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "WINREG: Failed to set %s=%d: error %d",
			   name, val, (int) GetLastError());
		return -1;
	}

	return 0;
}


static int wpa_config_write_reg_string(HKEY hk, const char *name,
				       const char *val)
{
	LONG ret;

	if (val == NULL) {
		RegDeleteValue(hk, name);
		return 0;
	}

	ret = RegSetValueEx(hk, name, 0, REG_SZ, val, strlen(val) + 1);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "WINREG: Failed to set %s='%s': "
			   "error %d", name, val, (int) GetLastError());
		return -1;
	}

	return 0;
}


static int wpa_config_write_global(struct wpa_config *config, HKEY hk)
{
#ifdef CONFIG_CTRL_IFACE
	wpa_config_write_reg_string(hk, "ctrl_interface",
				    config->ctrl_interface);
#endif /* CONFIG_CTRL_IFACE */

	wpa_config_write_reg_dword(hk, "eapol_version", config->eapol_version,
				   DEFAULT_EAPOL_VERSION);
	wpa_config_write_reg_dword(hk, "ap_scan", config->ap_scan,
				   DEFAULT_AP_SCAN);
	wpa_config_write_reg_dword(hk, "fast_reauth", config->fast_reauth,
				   DEFAULT_FAST_REAUTH);
	wpa_config_write_reg_dword(hk, "dot11RSNAConfigPMKLifetime",
				   config->dot11RSNAConfigPMKLifetime, 0);
	wpa_config_write_reg_dword(hk, "dot11RSNAConfigPMKReauthThreshold",
				   config->dot11RSNAConfigPMKReauthThreshold,
				   0);
	wpa_config_write_reg_dword(hk, "dot11RSNAConfigSATimeout",
				   config->dot11RSNAConfigSATimeout, 0);
	wpa_config_write_reg_dword(hk, "update_config", config->update_config,
				   0);
}


static int wpa_config_delete_subkeys(HKEY hk, const char *key)
{
	HKEY nhk;
	int i, errors = 0;
	LONG ret;

	ret = RegOpenKeyEx(hk, key, 0, KEY_ENUMERATE_SUB_KEYS | DELETE, &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not open key '%s' for "
			   "subkey deletion: error 0x%x (%d)", key,
			   (unsigned int) ret, (int) GetLastError());
		return 0;
	}

	for (i = 0; ; i++) {
		TCHAR name[255];
		DWORD namelen;

		namelen = 255;
		ret = RegEnumKeyEx(nhk, i, name, &namelen, NULL, NULL, NULL,
				   NULL);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegEnumKeyEx failed: 0x%x (%d)",
				   (unsigned int) ret, (int) GetLastError());
			break;
		}

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = '\0';

		ret = RegDeleteKey(nhk, name);
		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegDeleteKey failed: 0x%x (%d)",
				   (unsigned int) ret, (int) GetLastError());
			errors++;
		}
	}

	RegCloseKey(nhk);

	return errors ? -1 : 0;
}


static void write_str(HKEY hk, const char *field, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, field);
	if (value == NULL)
		return;
	wpa_config_write_reg_string(hk, field, value);
	free(value);
}


static void write_int(HKEY hk, const char *field, int value, int def)
{
	char val[20];
	if (value == def)
		return;
	snprintf(val, sizeof(val), "%d", value);
	wpa_config_write_reg_string(hk, field, val);
}


static void write_bssid(HKEY hk, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, "bssid");
	if (value == NULL)
		return;
	wpa_config_write_reg_string(hk, "bssid", value);
	free(value);
}


static void write_psk(HKEY hk, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, "psk");
	if (value == NULL)
		return;
	wpa_config_write_reg_string(hk, "psk", value);
	free(value);
}


static void write_proto(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->proto == DEFAULT_PROTO)
		return;

	value = wpa_config_get(ssid, "proto");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "proto", value);
	free(value);
}


static void write_key_mgmt(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->key_mgmt == DEFAULT_KEY_MGMT)
		return;

	value = wpa_config_get(ssid, "key_mgmt");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "key_mgmt", value);
	free(value);
}


static void write_pairwise(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->pairwise_cipher == DEFAULT_PAIRWISE)
		return;

	value = wpa_config_get(ssid, "pairwise");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "pairwise", value);
	free(value);
}


static void write_group(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->group_cipher == DEFAULT_GROUP)
		return;

	value = wpa_config_get(ssid, "group");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "group", value);
	free(value);
}


static void write_auth_alg(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->auth_alg == 0)
		return;

	value = wpa_config_get(ssid, "auth_alg");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "auth_alg", value);
	free(value);
}


#ifdef IEEE8021X_EAPOL
static void write_eap(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	value = wpa_config_get(ssid, "eap");
	if (value == NULL)
		return;

	if (value[0])
		wpa_config_write_reg_string(hk, "eap", value);
	free(value);
}
#endif /* IEEE8021X_EAPOL */


static void write_wep_key(HKEY hk, int idx, struct wpa_ssid *ssid)
{
	char field[20], *value;

	snprintf(field, sizeof(field), "wep_key%d", idx);
	value = wpa_config_get(ssid, field);
	if (value) {
		wpa_config_write_reg_string(hk, field, value);
		free(value);
	}
}


static int wpa_config_write_network(HKEY hk, struct wpa_ssid *ssid, int id)
{
	int i, errors = 0;
	HKEY nhk, netw;
	LONG ret;
	char name[5];

	ret = RegOpenKeyEx(hk, "networks", 0, KEY_CREATE_SUB_KEY, &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not open networks key "
			   "for subkey addition: error 0x%x (%d)",
			   (unsigned int) ret, (int) GetLastError());
		return 0;
	}

	snprintf(name, sizeof(name), "%04d", id);
	ret = RegCreateKeyEx(nhk, name, 0, NULL, 0, KEY_WRITE, NULL, &netw,
			     NULL);
	RegCloseKey(nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not add network key '%s':"
			   " error 0x%x (%d)",
			   name, (unsigned int) ret, (int) GetLastError());
		return -1;
	}

#define STR(t) write_str(netw, #t, ssid)
#define INT(t) write_int(netw, #t, ssid->t, 0)
#define INT_DEF(t, def) write_int(netw, #t, ssid->t, def)

	STR(ssid);
	INT(scan_ssid);
	write_bssid(netw, ssid);
	write_psk(netw, ssid);
	write_proto(netw, ssid);
	write_key_mgmt(netw, ssid);
	write_pairwise(netw, ssid);
	write_group(netw, ssid);
	write_auth_alg(netw, ssid);
#ifdef IEEE8021X_EAPOL
	write_eap(netw, ssid);
	STR(identity);
	STR(anonymous_identity);
	STR(eappsk);
	STR(nai);
	STR(password);
	STR(ca_cert);
	STR(client_cert);
	STR(private_key);
	STR(private_key_passwd);
	STR(dh_file);
	STR(subject_match);
	STR(altsubject_match);
	STR(ca_cert2);
	STR(client_cert2);
	STR(private_key2);
	STR(private_key2_passwd);
	STR(dh_file2);
	STR(subject_match2);
	STR(altsubject_match2);
	STR(phase1);
	STR(phase2);
	STR(pcsc);
	STR(pin);
	STR(engine_id);
	STR(key_id);
	INT(engine);
	INT_DEF(eapol_flags, DEFAULT_EAPOL_FLAGS);
#endif /* IEEE8021X_EAPOL */
	for (i = 0; i < 4; i++)
		write_wep_key(netw, i, ssid);
	INT(wep_tx_keyidx);
	INT(priority);
#ifdef IEEE8021X_EAPOL
	INT_DEF(eap_workaround, DEFAULT_EAP_WORKAROUND);
	STR(pac_file);
#endif /* IEEE8021X_EAPOL */
	INT(mode);
	INT(proactive_key_caching);
	INT(disabled);

#undef STR
#undef INT
#undef INT_DEF

	RegCloseKey(netw);

	return errors ? -1 : 0;
}


static int wpa_config_write_blob(HKEY hk, struct wpa_config_blob *blob)
{
	HKEY bhk;
	LONG ret;

	ret = RegCreateKeyEx(hk, "blobs", 0, NULL, 0, KEY_WRITE, NULL, &bhk,
			     NULL);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not add blobs key: "
			   "error 0x%x (%d)",
			   (unsigned int) ret, (int) GetLastError());
		return -1;
	}

	ret = RegSetValueEx(bhk, blob->name, 0, REG_BINARY, blob->data,
			    blob->len);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "WINREG: Failed to set blob %s': "
			   "error 0x%x (%d)", blob->name, (unsigned int) ret,
			   (int) GetLastError());
		RegCloseKey(bhk);
		return -1;
	}

	RegCloseKey(bhk);

	return 0;
}


int wpa_config_write(const char *name, struct wpa_config *config)
{
	char buf[256];
	HKEY hk;
	LONG ret;
	int errors = 0;
	struct wpa_ssid *ssid;
	struct wpa_config_blob *blob;
	int id;

	wpa_printf(MSG_DEBUG, "Writing configuration file '%s'", name);

	snprintf(buf, sizeof(buf), KEY_PREFIX "\\configs\\%s", name);
	ret = RegOpenKeyEx(KEY_ROOT, buf, 0, KEY_SET_VALUE | DELETE, &hk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "Could not open wpa_supplicant "
			   "configuration registry %s: error %d", buf,
			   (int) GetLastError());
		return -1;
	}

	if (wpa_config_write_global(config, hk)) {
		wpa_printf(MSG_ERROR, "Failed to write global configuration "
			   "data");
		errors++;
	}

	wpa_config_delete_subkeys(hk, "networks");
	for (ssid = config->ssid, id = 0; ssid; ssid = ssid->next, id++) {
		if (wpa_config_write_network(hk, ssid, id))
			errors++;
	}

	RegDeleteKey(hk, "blobs");
	for (blob = config->blobs; blob; blob = blob->next) {
		if (wpa_config_write_blob(hk, blob))
			errors++;
	}

	RegCloseKey(hk);

	wpa_printf(MSG_DEBUG, "Configuration '%s' written %ssuccessfully",
		   name, errors ? "un" : "");
	return errors ? -1 : 0;
}
