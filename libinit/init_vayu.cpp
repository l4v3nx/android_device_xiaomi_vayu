/*
   Copyright (c) 2015, The Linux Foundation. All rights reserved.
   Copyright (C) 2016 The CyanogenMod Project.
   Copyright (C) 2019-2020 The LineageOS Project.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.
   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdlib>
#include <fstream>
#include <string.h>
#include <unistd.h>
#include <vector>

#include <android-base/properties.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <sys/sysinfo.h>

#include "property_service.h"
#include "vendor_init.h"

#include <fs_mgr_dm_linear.h>

using android::base::GetProperty;
using std::string;

std::vector<std::string> ro_props_default_source_order = {
    "",
    "odm.",
    "product.",
    "system.",
    "system_ext.",
    "vendor.",
};

void property_override(char const prop[], char const value[], bool add = true) {
    prop_info *pi;

    pi = (prop_info *)__system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else if (add)
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void load_dalvik_properties() {
    struct sysinfo sys;

    sysinfo(&sys);
    if (sys.totalram < 6144ull * 1024 * 1024) {
        // from - phone-xhdpi-6144-dalvik-heap.mk
        property_override("dalvik.vm.heapstartsize", "16m");
        property_override("dalvik.vm.heapgrowthlimit", "256m");
        property_override("dalvik.vm.heapsize", "512m");
        property_override("dalvik.vm.heapmaxfree", "32m");
    } else {
        // 8GB & 12GB RAM
        property_override("dalvik.vm.heapstartsize", "32m");
        property_override("dalvik.vm.heapgrowthlimit", "512m");
        property_override("dalvik.vm.heapsize", "768m");
        property_override("dalvik.vm.heapmaxfree", "64m");
    }

    property_override("dalvik.vm.heaptargetutilization", "0.5");
    property_override("dalvik.vm.heapminfree", "8m");
}

void set_device_props(const std::string brand, const std::string device, const std::string model,
        const std::string name, const std::string marketname) {
    const auto set_ro_product_prop = [](const std::string &source,
                                        const std::string &prop,
                                        const std::string &value) {
        auto prop_name = "ro.product." + source + prop;
        property_override(prop_name.c_str(), value.c_str(), true);
    };

    for (const auto &source : ro_props_default_source_order) {
        set_ro_product_prop(source, "brand", brand);
        set_ro_product_prop(source, "device", device);
        set_ro_product_prop(source, "model", model);
        set_ro_product_prop(source, "name", name);
        set_ro_product_prop(source, "marketname", marketname);
    }
}

static const char *build_keys_props[] =
{
    "ro.build.tags",
    "ro.odm.build.tags",
    "ro.product.build.tags",
    "ro.system.build.tags",
    "ro.system_ext.build.tags",
    "ro.vendor.build.tags",
    nullptr};

/* From Magisk@native/jni/magiskhide/hide_utils.c */
static const char *cts_prop_key[] =
    {"ro.boot.vbmeta.device_state", "ro.boot.verifiedbootstate", "ro.boot.flash.locked",
     "ro.boot.veritymode", "ro.boot.warranty_bit", "ro.warranty_bit",
     "ro.debuggable", "ro.secure", "ro.build.type", "ro.build.tags",
     "ro.vendor.boot.warranty_bit", "ro.vendor.warranty_bit",
     "vendor.boot.vbmeta.device_state", nullptr};

static const char *cts_prop_val[] =
    {"locked", "green", "1",
     "enforcing", "0", "0",
     "0", "1", "user", "release-keys",
     "0", "0",
     "locked", nullptr};

static const char *cts_late_prop_key[] =
    {"vendor.boot.verifiedbootstate", nullptr};

static const char *cts_late_prop_val[] =
    {"green", nullptr};

static void workaround_cts_properties()
{
    // Hide all sensitive props
    for (int i = 0; cts_prop_key[i]; ++i)
    {
        property_override(cts_prop_key[i], cts_prop_val[i]);
    }
    for (int i = 0; cts_late_prop_key[i]; ++i)
    {
        property_override(cts_late_prop_key[i], cts_late_prop_val[i]);
    }
}

void vendor_load_properties() {
    string region = android::base::GetProperty("ro.boot.hwc", "");

    if (region == "INDIA") {
        set_device_props(
            "POCO", "bhima", "M2102J20SI", "bhima_global", "POCO X3 Pro");
        property_override("ro.product.mod_device", "bhima_global");
    } else {
        set_device_props(
            "Xiaomi", "vayu", "M2102J20SG", "vayu_global", "POCO X3 Pro");
        property_override("ro.product.mod_device", "vayu_global");
    }

    load_dalvik_properties();

//  SafetyNet workaround
    property_override("ro.boot.verifiedbootstate", "green");
//  Enable transitional log for Privileged permissions
    property_override("ro.control_privapp_permissions", "log");

    /* Spoof Build keys */
    for (int i = 0; build_keys_props[i]; ++i)
    {
        property_override(build_keys_props[i], "release-keys");
    }

    /* Workaround CTS */
    workaround_cts_properties();

#ifdef __ANDROID_RECOVERY__
    std::string buildtype = GetProperty("ro.build.type", "userdebug");
    if (buildtype != "user") {
        property_override("ro.debuggable", "1");
        property_override("ro.adb.secure.recovery", "0");
    }
#endif
}

