/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/*
 * Thomas Graf <tgraf@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2011 - 2012 Red Hat, Inc.
 */

#include <string.h>
#include <stdlib.h>
#include <dbus/dbus-glib.h>

#include "nm-setting-bond.h"
#include "nm-param-spec-specialized.h"
#include "nm-utils.h"
#include "nm-utils-private.h"
#include "nm-dbus-glib-types.h"

/**
 * SECTION:nm-setting-bond
 * @short_description: Describes connection properties for bonds
 * @include: nm-setting-bond.h
 *
 * The #NMSettingBond object is a #NMSetting subclass that describes properties
 * necessary for bond connections.
 **/

/**
 * nm_setting_bond_error_quark:
 *
 * Registers an error quark for #NMSettingBond if necessary.
 *
 * Returns: the error quark used for #NMSettingBond errors.
 **/
GQuark
nm_setting_bond_error_quark (void)
{
	static GQuark quark;

	if (G_UNLIKELY (!quark))
		quark = g_quark_from_static_string ("nm-setting-bond-error-quark");
	return quark;
}


G_DEFINE_TYPE (NMSettingBond, nm_setting_bond, NM_TYPE_SETTING)

#define NM_SETTING_BOND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_SETTING_BOND, NMSettingBondPrivate))

typedef struct {
	char *interface_name;
	GHashTable *options;
} NMSettingBondPrivate;

enum {
	PROP_0,
	PROP_INTERFACE_NAME,
	PROP_OPTIONS,
	LAST_PROP
};

typedef struct {
	const char *opt;
	const char *val;
} BondDefault;

static const BondDefault defaults[] = {
	{ NM_SETTING_BOND_OPTION_MODE,          "balance-rr" },
	{ NM_SETTING_BOND_OPTION_MIIMON,        "100"        },
	{ NM_SETTING_BOND_OPTION_DOWNDELAY,     "0"          },
	{ NM_SETTING_BOND_OPTION_UPDELAY,       "0"          },
	{ NM_SETTING_BOND_OPTION_ARP_INTERVAL,  "0"          },
	{ NM_SETTING_BOND_OPTION_ARP_IP_TARGET, ""           },
};

/**
 * nm_setting_bond_new:
 *
 * Creates a new #NMSettingBond object with default values.
 *
 * Returns: (transfer full): the new empty #NMSettingBond object
 **/
NMSetting *
nm_setting_bond_new (void)
{
	return (NMSetting *) g_object_new (NM_TYPE_SETTING_BOND, NULL);
}

/**
 * nm_setting_bond_get_interface_name:
 * @setting: the #NMSettingBond
 *
 * Returns: the #NMSettingBond:interface-name property of the setting
 **/
const char *
nm_setting_bond_get_interface_name (NMSettingBond *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_BOND (setting), NULL);

	return NM_SETTING_BOND_GET_PRIVATE (setting)->interface_name;
}

/**
 * nm_setting_bond_get_num_options:
 * @setting: the #NMSettingBond
 *
 * Returns the number of options that should be set for this bond when it
 * is activated. This can be used to retrieve each option individually
 * using nm_setting_bond_get_option().
 *
 * Returns: the number of bonding options
 **/
guint32
nm_setting_bond_get_num_options (NMSettingBond *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_BOND (setting), 0);

	return g_hash_table_size (NM_SETTING_BOND_GET_PRIVATE (setting)->options);
}

/**
 * nm_setting_bond_get_option:
 * @setting: the #NMSettingBond
 * @idx: index of the desired option, from 0 to
 * nm_setting_bond_get_num_options() - 1
 * @out_name: (out): on return, the name of the bonding option; this
 * value is owned by the setting and should not be modified
 * @out_value: (out): on return, the value of the name of the bonding
 * option; this value is owned by the setting and should not be modified
 *
 * Given an index, return the value of the bonding option at that index.  Indexes
 * are *not* guaranteed to be static across modifications to options done by
 * nm_setting_bond_add_option() and nm_setting_bond_remove_option(),
 * and should not be used to refer to options except for short periods of time
 * such as during option iteration.
 *
 * Returns: %TRUE on success if the index was valid and an option was found,
 * %FALSE if the index was invalid (ie, greater than the number of options
 * currently held by the setting)
 **/
gboolean
nm_setting_bond_get_option (NMSettingBond *setting,
                            guint32 idx,
                            const char **out_name,
                            const char **out_value)
{
	NMSettingBondPrivate *priv;
	GList *keys;
	const char *_key = NULL, *_value = NULL;

	g_return_val_if_fail (NM_IS_SETTING_BOND (setting), FALSE);

	priv = NM_SETTING_BOND_GET_PRIVATE (setting);

	if (idx >= nm_setting_bond_get_num_options (setting))
		return FALSE;

	keys = g_hash_table_get_keys (priv->options);
	_key = g_list_nth_data (keys, idx);
	_value = g_hash_table_lookup (priv->options, _key);

	if (out_name)
		*out_name = _key;
	if (out_value)
		*out_value = _value;

	g_list_free (keys);
	return TRUE;
}

static gboolean
validate_option (const char *name)
{
	guint i;

	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (name[0] != '\0', FALSE);

	for (i = 0; i < G_N_ELEMENTS (defaults); i++) {
		if (g_strcmp0 (defaults[i].opt, name) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * nm_setting_bond_get_option_by_name:
 * @setting: the #NMSettingBond
 * @name: the option name for which to retrieve the value
 *
 * Returns the value associated with the bonding option specified by
 * @name, if it exists.
 *
 * Returns: the value, or %NULL if the key/value pair was never added to the
 * setting; the value is owned by the setting and must not be modified
 **/
const char *
nm_setting_bond_get_option_by_name (NMSettingBond *setting,
                                    const char *name)
{
	g_return_val_if_fail (NM_IS_SETTING_BOND (setting), NULL);
	g_return_val_if_fail (validate_option (name), NULL);

	return g_hash_table_lookup (NM_SETTING_BOND_GET_PRIVATE (setting)->options, name);
}

/**
 * nm_setting_bond_add_option:
 * @setting: the #NMSettingBond
 * @name: name for the option
 * @value: value for the option
 *
 * Add an option to the table.  The option is compared to an internal list
 * of allowed options.  Option names may contain only alphanumeric characters
 * (ie [a-zA-Z0-9]).  Adding a new name replaces any existing name/value pair
 * that may already exist.
 *
 * Returns: %TRUE if the option was valid and was added to the internal option
 * list, %FALSE if it was not.
 **/
gboolean nm_setting_bond_add_option (NMSettingBond *setting,
                                     const char *name,
                                     const char *value)
{
	NMSettingBondPrivate *priv;
	size_t value_len;

	g_return_val_if_fail (NM_IS_SETTING_BOND (setting), FALSE);
	g_return_val_if_fail (validate_option (name), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	priv = NM_SETTING_BOND_GET_PRIVATE (setting);

	value_len = strlen (value);
	g_return_val_if_fail (value_len > 0 && value_len < 200, FALSE);

	g_hash_table_insert (priv->options, g_strdup (name), g_strdup (value));

	if (!strcmp (name, NM_SETTING_BOND_OPTION_MIIMON)) {
		g_hash_table_remove (priv->options, NM_SETTING_BOND_OPTION_ARP_INTERVAL);
		g_hash_table_remove (priv->options, NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
	} else if (!strcmp (name, NM_SETTING_BOND_OPTION_ARP_INTERVAL))
		g_hash_table_remove (priv->options, NM_SETTING_BOND_OPTION_MIIMON);

	return TRUE;
}

/**
 * nm_setting_bond_remove_option:
 * @setting: the #NMSettingBond
 * @name: name of the option to remove
 *
 * Remove the bonding option referenced by @name from the internal option
 * list.
 *
 * Returns: %TRUE if the option was found and removed from the internal option
 * list, %FALSE if it was not.
 **/
gboolean
nm_setting_bond_remove_option (NMSettingBond *setting,
                               const char *name)
{
	g_return_val_if_fail (NM_IS_SETTING_BOND (setting), FALSE);
	g_return_val_if_fail (validate_option (name), FALSE);

	return g_hash_table_remove (NM_SETTING_BOND_GET_PRIVATE (setting)->options, name);
}

/**
 * nm_setting_bond_get_valid_options:
 * @setting: the #NMSettingBond
 *
 * Returns a list of valid bond options.
 *
 * Returns: (transfer none): a NULL-terminated array of strings of valid bond options.
 **/
const char **
nm_setting_bond_get_valid_options  (NMSettingBond *setting)
{
	static const char *array[G_N_ELEMENTS (defaults) + 1] = { NULL };
	int i;

	/* initialize the array once */
	if (G_UNLIKELY (array[0] == NULL)) {
		for (i = 0; i < G_N_ELEMENTS (defaults); i++)
			array[i] = defaults[i].opt;
		array[i] = NULL;
	}
	return array;
}

/**
 * nm_setting_bond_get_option_default:
 * @setting: the #NMSettingBond
 * @name: the name of the option
 *
 * Returns: the value of the bond option if not overridden by an entry in
 *   the #NMSettingBond:options property.
 **/
const char *
nm_setting_bond_get_option_default (NMSettingBond *setting, const char *name)
{
	guint i;

	g_return_val_if_fail (NM_IS_SETTING_BOND (setting), NULL);
	g_return_val_if_fail (validate_option (name), NULL);

	for (i = 0; i < G_N_ELEMENTS (defaults); i++) {
		if (g_strcmp0 (defaults[i].opt, name) == 0)
			return defaults[i].val;
	}
	/* Any option that passes validate_option() should also be found in defaults */
	g_assert_not_reached ();
}

static gint
find_setting_by_name (gconstpointer a, gconstpointer b)
{
	NMSetting *setting = NM_SETTING (a);
	const char *str = (const char *) b;

	return strcmp (nm_setting_get_name (setting), str);
}

static gboolean
verify (NMSetting *setting, GSList *all_settings, GError **error)
{
	NMSettingBondPrivate *priv = NM_SETTING_BOND_GET_PRIVATE (setting);
	GHashTableIter iter;
	const char *key, *value;
	const char *valid_modes[] = { "balance-rr",
	                              "active-backup",
	                              "balance-xor",
	                              "broadcast",
	                              "802.3ad",
	                              "balance-tlb",
	                              "balance-alb",
	                              NULL };
	int miimon = 0, arp_interval = 0;
	const char *arp_ip_target = NULL;

	if (!priv->interface_name || !strlen(priv->interface_name)) {
		g_set_error (error,
		             NM_SETTING_BOND_ERROR,
		             NM_SETTING_BOND_ERROR_MISSING_PROPERTY,
		             NM_SETTING_BOND_INTERFACE_NAME);
		return FALSE;
	}

	if (!nm_utils_iface_valid_name (priv->interface_name)) {
		g_set_error (error,
		             NM_SETTING_BOND_ERROR,
		             NM_SETTING_BOND_ERROR_INVALID_PROPERTY,
		             NM_SETTING_BOND_INTERFACE_NAME);
		return FALSE;
	}

	g_hash_table_iter_init (&iter, priv->options);
	while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &value)) {
		if (   !validate_option (key)
		    || !value[0]
		    || (strlen (value) > 200)
		    || strchr (value, ' ')) {
			g_set_error_literal (error,
			                     NM_SETTING_BOND_ERROR,
			                     NM_SETTING_BOND_ERROR_INVALID_OPTION,
			                     key);
			return FALSE;
		}
	}

	value = g_hash_table_lookup (priv->options, NM_SETTING_BOND_OPTION_MIIMON);
	if (value)
		miimon = atoi (value);
	value = g_hash_table_lookup (priv->options, NM_SETTING_BOND_OPTION_ARP_INTERVAL);
	if (value)
		arp_interval = atoi (value);

	/* Can only set one of miimon and arp_interval */
	if (miimon > 0 && arp_interval > 0) {
		g_set_error (error,
		             NM_SETTING_BOND_ERROR,
		             NM_SETTING_BOND_ERROR_INVALID_OPTION,
		             NM_SETTING_BOND_OPTION_ARP_INTERVAL);
	}

	value = g_hash_table_lookup (priv->options, NM_SETTING_BOND_OPTION_MODE);
	if (!value) {
		g_set_error (error,
		             NM_SETTING_BOND_ERROR,
		             NM_SETTING_BOND_ERROR_MISSING_OPTION,
		             NM_SETTING_BOND_OPTION_MODE);
		return FALSE;
	}
	if (!_nm_utils_string_in_list (value, valid_modes)) {
		g_set_error (error,
		             NM_SETTING_BOND_ERROR,
		             NM_SETTING_BOND_ERROR_INVALID_OPTION,
		             NM_SETTING_BOND_OPTION_MODE);
		return FALSE;
	}

	/* Make sure mode is compatible with other settings */
	if (   strcmp (value, "balance-alb") == 0
	    || strcmp (value, "balance-tlb") == 0) {
		if (arp_interval > 0) {
			g_set_error (error,
			             NM_SETTING_BOND_ERROR,
			             NM_SETTING_BOND_ERROR_INVALID_OPTION,
			             NM_SETTING_BOND_OPTION_ARP_INTERVAL);
		}
	}
	if (g_slist_find_custom (all_settings, NM_SETTING_INFINIBAND_SETTING_NAME, find_setting_by_name)) {
		if (strcmp (value, "active-backup") != 0) {
			g_set_error (error,
			             NM_SETTING_BOND_ERROR,
			             NM_SETTING_BOND_ERROR_INVALID_OPTION,
			             NM_SETTING_BOND_OPTION_MODE);
			return FALSE;
		}
	}

	if (miimon == 0) {
		/* updelay and downdelay can only be used with miimon */
		if (g_hash_table_lookup (priv->options, NM_SETTING_BOND_OPTION_UPDELAY)) {
			g_set_error (error,
			             NM_SETTING_BOND_ERROR,
			             NM_SETTING_BOND_ERROR_INVALID_OPTION,
			             NM_SETTING_BOND_OPTION_UPDELAY);
			return FALSE;
		}
		if (g_hash_table_lookup (priv->options, NM_SETTING_BOND_OPTION_DOWNDELAY)) {
			g_set_error (error,
			             NM_SETTING_BOND_ERROR,
			             NM_SETTING_BOND_ERROR_INVALID_OPTION,
			             NM_SETTING_BOND_OPTION_DOWNDELAY);
			return FALSE;
		}
	}

	/* arp_ip_target can only be used with arp_interval, and must
	 * contain a comma-separated list of IPv4 addresses.
	 */
	arp_ip_target = g_hash_table_lookup (priv->options, NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
	if (arp_interval > 0) {
		char **addrs;
		guint32 addr;
		int i;

		if (!arp_ip_target) {
			g_set_error (error,
			             NM_SETTING_BOND_ERROR,
			             NM_SETTING_BOND_ERROR_MISSING_OPTION,
			             NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
			return FALSE;
		}

		addrs = g_strsplit (arp_ip_target, ",", -1);
		if (!addrs[0]) {
			g_strfreev (addrs);
			g_set_error (error,
			             NM_SETTING_BOND_ERROR,
			             NM_SETTING_BOND_ERROR_INVALID_OPTION,
			             NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
			return FALSE;
		}

		for (i = 0; addrs[i]; i++) {
			if (!inet_pton (AF_INET, addrs[i], &addr)) {
				g_strfreev (addrs);
				g_set_error (error,
				             NM_SETTING_BOND_ERROR,
				             NM_SETTING_BOND_ERROR_INVALID_OPTION,
				             NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
				return FALSE;
			}
		}
		g_strfreev (addrs);
	} else {
		if (arp_ip_target) {
			g_set_error (error,
			             NM_SETTING_BOND_ERROR,
			             NM_SETTING_BOND_ERROR_INVALID_OPTION,
			             NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
			return FALSE;
		}
	}

	return TRUE;
}

static const char *
get_virtual_iface_name (NMSetting *setting)
{
	NMSettingBond *self = NM_SETTING_BOND (setting);

	return nm_setting_bond_get_interface_name (self);
}

static void
nm_setting_bond_init (NMSettingBond *setting)
{
	NMSettingBondPrivate *priv = NM_SETTING_BOND_GET_PRIVATE (setting);

	g_object_set (setting, NM_SETTING_NAME, NM_SETTING_BOND_SETTING_NAME,
	              NULL);

	priv->options = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	/* Default values: */
	nm_setting_bond_add_option (setting, NM_SETTING_BOND_OPTION_MODE, "balance-rr");
	nm_setting_bond_add_option (setting, NM_SETTING_BOND_OPTION_MIIMON, "100");
}

static void
finalize (GObject *object)
{
	NMSettingBondPrivate *priv = NM_SETTING_BOND_GET_PRIVATE (object);

	g_free (priv->interface_name);
	g_hash_table_destroy (priv->options);

	G_OBJECT_CLASS (nm_setting_bond_parent_class)->finalize (object);
}

static void
copy_hash (gpointer key, gpointer value, gpointer user_data)
{
	g_hash_table_insert ((GHashTable *) user_data, g_strdup (key), g_strdup (value));
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	NMSettingBondPrivate *priv = NM_SETTING_BOND_GET_PRIVATE (object);
	GHashTable *new_hash;

	switch (prop_id) {
	case PROP_INTERFACE_NAME:
		priv->interface_name = g_value_dup_string (value);
		break;
	case PROP_OPTIONS:
		/* Must make a deep copy of the hash table here... */
		g_hash_table_remove_all (priv->options);
		new_hash = g_value_get_boxed (value);
		if (new_hash)
			g_hash_table_foreach (new_hash, copy_hash, priv->options);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	NMSettingBondPrivate *priv = NM_SETTING_BOND_GET_PRIVATE (object);
	NMSettingBond *setting = NM_SETTING_BOND (object);

	switch (prop_id) {
	case PROP_INTERFACE_NAME:
		g_value_set_string (value, nm_setting_bond_get_interface_name (setting));
		break;
	case PROP_OPTIONS:
		g_value_set_boxed (value, priv->options);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_setting_bond_class_init (NMSettingBondClass *setting_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (setting_class);
	NMSettingClass *parent_class = NM_SETTING_CLASS (setting_class);

	g_type_class_add_private (setting_class, sizeof (NMSettingBondPrivate));

	/* virtual methods */
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	object_class->finalize     = finalize;
	parent_class->verify       = verify;
	parent_class->get_virtual_iface_name = get_virtual_iface_name;

	/* Properties */
	/**
	 * NMSettingBond:interface-name:
	 *
	 * The name of the virtual in-kernel bonding network interface
	 **/
	g_object_class_install_property
		(object_class, PROP_INTERFACE_NAME,
		 g_param_spec_string (NM_SETTING_BOND_INTERFACE_NAME,
		                      "InterfaceName",
		                      "The name of the virtual in-kernel bonding network interface",
		                      NULL,
		                      G_PARAM_READWRITE | NM_SETTING_PARAM_SERIALIZE));

	/**
	 * NMSettingBond:options:
	 *
	 * Dictionary of key/value pairs of bonding options.  Both keys
	 * and values must be strings. Option names must contain only
	 * alphanumeric characters (ie, [a-zA-Z0-9]).
	 **/
	 g_object_class_install_property
		 (object_class, PROP_OPTIONS,
		 _nm_param_spec_specialized (NM_SETTING_BOND_OPTIONS,
		                             "Options",
		                             "Dictionary of key/value pairs of bonding "
		                             "options.  Both keys and values must be "
		                             "strings.  Option names must contain only "
		                             "alphanumeric characters (ie, [a-zA-Z0-9]).",
		                             DBUS_TYPE_G_MAP_OF_STRING,
		                             G_PARAM_READWRITE | NM_SETTING_PARAM_SERIALIZE));
}
