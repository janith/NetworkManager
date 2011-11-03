/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2011 Red Hat, Inc.
 */

#include <string.h>
#include <glib.h>
#include <dbus/dbus.h>

#include "nm-firewall-manager.h"
#include "nm-dbus-manager.h"
#include "nm-logging.h"
#include "nm-dbus-glib-types.h"

#define NM_FIREWALL_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                              NM_TYPE_FIREWALL_MANAGER, \
                                              NMFirewallManagerPrivate))

G_DEFINE_TYPE (NMFirewallManager, nm_firewall_manager, G_TYPE_OBJECT)

/* Properties */
enum {
	PROP_0 = 0,
	PROP_AVAILABLE,
	LAST_PROP
};

typedef struct {
	NMDBusManager * dbus_mgr;
	guint           name_owner_id;
	DBusGProxy *    proxy;
	gboolean        running;
	gboolean        disposed;
} NMFirewallManagerPrivate;

/********************************************************************/

DBusGProxyCall *
nm_firewall_manager_add_to_zone (NMFirewallManager *self,
                                 const char *ip_iface,
                                 const char *zone,
                                 DBusGProxyCallNotify callback,
                                 gpointer callback_data)
{
	NMFirewallManagerPrivate *priv = NM_FIREWALL_MANAGER_GET_PRIVATE (self);
	DBusGProxyCall * call = NULL;

	if (nm_firewall_manager_available (self)) {
		nm_log_dbg (LOGD_DEVICE, "(%s) adding to firewall zone: %s", ip_iface, zone );
		call = dbus_g_proxy_begin_call_with_timeout (priv->proxy,
		                                             "AddInterface",
		                                             callback,
		                                             callback_data, /* NMDevice */
		                                             NULL, /* destroy callback_data */
		                                             10000,      /* timeout */
		                                             G_TYPE_STRING, ip_iface,
		                                             G_TYPE_STRING, zone ? zone : "",
		                                             DBUS_TYPE_G_MAP_OF_VARIANT, NULL, /* a{sv}:options */
		                                             G_TYPE_INVALID);
	} else {
		nm_log_dbg (LOGD_DEVICE, "Firewall zone add skipped because firewall isn't running");
		callback (NULL, NULL, callback_data);
	}

	return call;
}

void nm_firewall_manager_cancel_add (NMFirewallManager *self, DBusGProxyCall * fw_call)
{
	NMFirewallManagerPrivate *priv = NM_FIREWALL_MANAGER_GET_PRIVATE (self);

	dbus_g_proxy_cancel_call (priv->proxy, fw_call);
}

gboolean
nm_firewall_manager_available (NMFirewallManager *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (NM_IS_FIREWALL_MANAGER (self), FALSE);

	return NM_FIREWALL_MANAGER_GET_PRIVATE (self)->running;
}

static void
set_running (NMFirewallManager *self, gboolean now_running)
{
	NMFirewallManagerPrivate *priv = NM_FIREWALL_MANAGER_GET_PRIVATE (self);
	gboolean old_available = nm_firewall_manager_available (self);

	priv->running = now_running;
	if (old_available != nm_firewall_manager_available (self))
		g_object_notify (G_OBJECT (self), NM_FIREWALL_MANAGER_AVAILABLE);
}

static void
name_owner_changed (NMDBusManager *dbus_mgr,
                    const char *name,
                    const char *old_owner,
                    const char *new_owner,
                    gpointer user_data)
{
	NMFirewallManager *self = NM_FIREWALL_MANAGER (user_data);
	gboolean old_owner_good = (old_owner && strlen (old_owner));
	gboolean new_owner_good = (new_owner && strlen (new_owner));

	/* We only care about the firewall here */
	if (strcmp (FIREWALL_DBUS_SERVICE, name) != 0)
		return;

	if (!old_owner_good && new_owner_good) {
		nm_log_dbg (LOGD_DEVICE, "firewall started");
		set_running (self, TRUE);
	} else if (old_owner_good && !new_owner_good) {
		nm_log_dbg (LOGD_DEVICE, "firewall stopped");
		set_running (self, FALSE);
	}
}

/*******************************************************************/

NMFirewallManager *
nm_firewall_manager_get (void)
{
	static NMFirewallManager *singleton = NULL;

	if (!singleton)
		singleton = NM_FIREWALL_MANAGER (g_object_new (NM_TYPE_FIREWALL_MANAGER, NULL));
	else
		g_object_ref (singleton);

	g_assert (singleton);
	return singleton;
}

static void
nm_firewall_manager_init (NMFirewallManager * self)
{
	NMFirewallManagerPrivate *priv = NM_FIREWALL_MANAGER_GET_PRIVATE (self);
	DBusGConnection *bus;

	priv->dbus_mgr = nm_dbus_manager_get ();
	priv->name_owner_id = g_signal_connect (priv->dbus_mgr,
	                                        NM_DBUS_MANAGER_NAME_OWNER_CHANGED,
	                                        G_CALLBACK (name_owner_changed),
	                                        self);
	priv->running = nm_dbus_manager_name_has_owner (priv->dbus_mgr, FIREWALL_DBUS_SERVICE);
	nm_log_dbg (LOGD_DEVICE, "firewall is %s running", priv->running ? "" : "not" );

	bus = nm_dbus_manager_get_connection (priv->dbus_mgr);
	priv->proxy = dbus_g_proxy_new_for_name (bus,
	                                         FIREWALL_DBUS_SERVICE,
	                                         FIREWALL_DBUS_PATH,
	                                         FIREWALL_DBUS_INTERFACE);
}

static void
set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_AVAILABLE:
		g_value_set_boolean (value, nm_firewall_manager_available (NM_FIREWALL_MANAGER (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
dispose (GObject *object)
{
	NMFirewallManagerPrivate *priv = NM_FIREWALL_MANAGER_GET_PRIVATE (object);

	if (priv->disposed)
		goto out;
	priv->disposed = TRUE;

	if (priv->dbus_mgr) {
		if (priv->name_owner_id)
			g_signal_handler_disconnect (priv->dbus_mgr, priv->name_owner_id);
		g_object_unref (G_OBJECT (priv->dbus_mgr));
	}

	if (priv->proxy)
		g_object_unref (priv->proxy);

out:
	/* Chain up to the parent class */
	G_OBJECT_CLASS (nm_firewall_manager_parent_class)->dispose (object);
}

static void
nm_firewall_manager_class_init (NMFirewallManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (NMFirewallManagerPrivate));

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;

	g_object_class_install_property (object_class, PROP_AVAILABLE,
		g_param_spec_boolean (NM_FIREWALL_MANAGER_AVAILABLE,
		                      "Available",
		                      "Available",
		                      FALSE,
		                      G_PARAM_READABLE));
}

