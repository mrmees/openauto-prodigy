# Multi-Vehicle Companion App Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Allow the companion app to pair with multiple OpenAuto Prodigy head units and auto-connect based on WiFi SSID.

**Architecture:** Replace single-vehicle prefs with a JSON vehicle list keyed by SSID. WifiMonitor matches current WiFi against all paired vehicles. Pi installer generates UUID-suffixed default SSID for uniqueness.

**Tech Stack:** Kotlin, Jetpack Compose, SharedPreferences (JSON), Android ConnectivityManager, bash (install.sh)

**Repos:**
- Companion app: `/home/matt/claude/personal/openautopro/companion-app/`
- Prodigy (Pi): `/home/matt/claude/personal/openautopro/openauto-prodigy/`

**Build/test commands:**
- Companion: `cd /home/matt/claude/personal/openautopro/companion-app && ./gradlew assembleDebug`
- Companion tests: `cd /home/matt/claude/personal/openautopro/companion-app && ./gradlew test`
- Install on phone: `/home/matt/claude/personal/openautopro/openauto-prodigy/platform-tools/adb install -r /home/matt/claude/personal/openautopro/companion-app/app/build/outputs/apk/debug/app-debug.apk`

**Design doc:** `docs/plans/2026-02-22-multi-vehicle-companion-design.md`

---

### Task 1: Vehicle data model and CompanionPrefs multi-vehicle storage

**Files:**
- Create: `companion-app/app/src/main/java/org/openauto/companion/data/Vehicle.kt`
- Modify: `companion-app/app/src/main/java/org/openauto/companion/data/CompanionPrefs.kt`
- Create: `companion-app/app/src/test/java/org/openauto/companion/data/CompanionPrefsTest.kt`

**Step 1: Create Vehicle data class**

Create `companion-app/app/src/main/java/org/openauto/companion/data/Vehicle.kt`:

```kotlin
package org.openauto.companion.data

import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

data class Vehicle(
    val id: String = UUID.randomUUID().toString().take(8),
    val ssid: String,
    val name: String = ssid,
    val sharedSecret: String,
    val socks5Enabled: Boolean = true
) {
    fun toJson(): JSONObject = JSONObject().apply {
        put("id", id)
        put("ssid", ssid)
        put("name", name)
        put("shared_secret", sharedSecret)
        put("socks5_enabled", socks5Enabled)
    }

    companion object {
        fun fromJson(json: JSONObject): Vehicle = Vehicle(
            id = json.optString("id", UUID.randomUUID().toString().take(8)),
            ssid = json.getString("ssid"),
            name = json.optString("name", json.getString("ssid")),
            sharedSecret = json.getString("shared_secret"),
            socks5Enabled = json.optBoolean("socks5_enabled", true)
        )

        fun listToJson(vehicles: List<Vehicle>): String =
            JSONArray(vehicles.map { it.toJson() }).toString()

        fun listFromJson(json: String): List<Vehicle> {
            if (json.isBlank()) return emptyList()
            val arr = JSONArray(json)
            return (0 until arr.length()).map { fromJson(arr.getJSONObject(it)) }
        }

        const val MAX_VEHICLES = 20
    }
}
```

**Step 2: Rewrite CompanionPrefs for multi-vehicle**

Replace `companion-app/app/src/main/java/org/openauto/companion/data/CompanionPrefs.kt` entirely:

```kotlin
package org.openauto.companion.data

import android.content.Context
import android.content.SharedPreferences
import android.util.Log

class CompanionPrefs(context: Context) {
    private val prefs: SharedPreferences =
        context.getSharedPreferences("companion", Context.MODE_PRIVATE)

    init { migrateIfNeeded() }

    var vehicles: List<Vehicle>
        get() = Vehicle.listFromJson(prefs.getString("vehicles_json", "") ?: "")
        set(value) = prefs.edit().putString("vehicles_json", Vehicle.listToJson(value)).apply()

    val isPaired: Boolean get() = vehicles.isNotEmpty()

    fun findBySsid(ssid: String): Vehicle? = vehicles.find { it.ssid == ssid }

    fun addVehicle(vehicle: Vehicle): Boolean {
        val current = vehicles
        if (current.size >= Vehicle.MAX_VEHICLES) return false
        if (current.any { it.ssid == vehicle.ssid }) return false
        vehicles = current + vehicle
        return true
    }

    fun removeVehicle(id: String) {
        vehicles = vehicles.filter { it.id != id }
    }

    fun updateVehicle(id: String, transform: (Vehicle) -> Vehicle) {
        vehicles = vehicles.map { if (it.id == id) transform(it) else it }
    }

    /** Migrate legacy single-vehicle prefs to vehicle list (idempotent). */
    private fun migrateIfNeeded() {
        if (prefs.contains("vehicles_json")) return
        val secret = prefs.getString("shared_secret", "") ?: ""
        if (secret.isEmpty()) return

        val ssid = prefs.getString("target_ssid", "OpenAutoProdigy") ?: "OpenAutoProdigy"
        val socks5 = prefs.getBoolean("socks5_enabled", true)
        val vehicle = Vehicle(ssid = ssid, sharedSecret = secret, socks5Enabled = socks5)
        vehicles = listOf(vehicle)
        Log.i("CompanionPrefs", "Migrated legacy prefs to vehicle: ssid=$ssid")

        // Clean up legacy keys
        prefs.edit()
            .remove("shared_secret")
            .remove("target_ssid")
            .remove("socks5_enabled")
            .apply()
    }
}
```

**Step 3: Write unit tests**

Create `companion-app/app/src/test/java/org/openauto/companion/data/CompanionPrefsTest.kt`:

```kotlin
package org.openauto.companion.data

import org.json.JSONArray
import org.junit.Assert.*
import org.junit.Test

class VehicleSerializationTest {
    @Test
    fun roundTrip_singleVehicle() {
        val v = Vehicle(id = "abc123", ssid = "MiataAA-A3F2", name = "Miata",
            sharedSecret = "deadbeef", socks5Enabled = true)
        val json = Vehicle.listToJson(listOf(v))
        val result = Vehicle.listFromJson(json)
        assertEquals(1, result.size)
        assertEquals("abc123", result[0].id)
        assertEquals("MiataAA-A3F2", result[0].ssid)
        assertEquals("Miata", result[0].name)
        assertEquals("deadbeef", result[0].sharedSecret)
        assertTrue(result[0].socks5Enabled)
    }

    @Test
    fun roundTrip_multipleVehicles() {
        val vehicles = listOf(
            Vehicle(id = "a1", ssid = "Car1", sharedSecret = "s1"),
            Vehicle(id = "b2", ssid = "Car2", name = "Truck", sharedSecret = "s2", socks5Enabled = false)
        )
        val result = Vehicle.listFromJson(Vehicle.listToJson(vehicles))
        assertEquals(2, result.size)
        assertEquals("Car2", result[1].ssid)
        assertEquals("Truck", result[1].name)
        assertFalse(result[1].socks5Enabled)
    }

    @Test
    fun fromJson_defaultsNameToSsid() {
        val json = org.json.JSONObject().apply {
            put("ssid", "TestAP")
            put("shared_secret", "abc")
        }
        val v = Vehicle.fromJson(json)
        assertEquals("TestAP", v.name)
    }

    @Test
    fun emptyJson_returnsEmptyList() {
        assertEquals(emptyList<Vehicle>(), Vehicle.listFromJson(""))
    }
}
```

**Step 4: Run tests**

Run: `cd /home/matt/claude/personal/openautopro/companion-app && ./gradlew test`
Expected: All tests pass including new VehicleSerializationTest.

**Step 5: Commit**

```bash
cd /home/matt/claude/personal/openautopro/companion-app
git add app/src/main/java/org/openauto/companion/data/Vehicle.kt \
        app/src/main/java/org/openauto/companion/data/CompanionPrefs.kt \
        app/src/test/java/org/openauto/companion/data/CompanionPrefsTest.kt
git commit -m "feat: multi-vehicle storage with Vehicle model and migration"
```

---

### Task 2: WifiMonitor multi-SSID matching

**Files:**
- Modify: `companion-app/app/src/main/java/org/openauto/companion/service/WifiMonitor.kt`
- Modify: `companion-app/app/src/main/java/org/openauto/companion/CompanionApp.kt`

**Step 1: Rewrite WifiMonitor for multi-vehicle**

Replace constructor and matching logic in `WifiMonitor.kt`. The monitor now takes a list of vehicles and matches against all SSIDs:

```kotlin
package org.openauto.companion.service

import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiInfo
import android.net.wifi.WifiManager
import android.util.Log
import androidx.core.content.ContextCompat
import org.openauto.companion.data.Vehicle

class WifiMonitor(
    private val context: Context,
    private val vehicles: List<Vehicle>
) {
    private val connectivityManager =
        context.getSystemService(ConnectivityManager::class.java)
    private var registered = false
    private var wifiNetwork: Network? = null
    private var activeVehicle: Vehicle? = null
    private val ssidMap: Map<String, Vehicle> = vehicles.associateBy { it.ssid }

    private val networkCallback = object : ConnectivityManager.NetworkCallback(
        FLAG_INCLUDE_LOCATION_INFO
    ) {
        override fun onCapabilitiesChanged(network: Network, caps: NetworkCapabilities) {
            val wifiInfo = caps.transportInfo as? WifiInfo ?: return
            val ssid = wifiInfo.ssid?.removeSurrounding("\"") ?: return
            if (ssid == "<unknown ssid>") return

            val vehicle = ssidMap[ssid] ?: return
            if (activeVehicle?.ssid == ssid) return // already connected to this one

            Log.i(TAG, "Matched vehicle '${vehicle.name}' on SSID: $ssid")
            wifiNetwork = network
            activeVehicle = vehicle
            startCompanionService(vehicle)
        }

        override fun onLost(network: Network) {
            Log.i(TAG, "WiFi network lost")
            wifiNetwork = null
            activeVehicle = null
            stopCompanionService()
        }
    }

    fun start() {
        if (registered) return
        if (vehicles.isEmpty()) return
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .build()
        connectivityManager.registerNetworkCallback(request, networkCallback)
        registered = true
        Log.i(TAG, "WiFi monitor started, watching for ${vehicles.size} vehicle(s): ${vehicles.map { it.ssid }}")
        checkCurrentNetwork()
    }

    private fun checkCurrentNetwork() {
        try {
            @Suppress("DEPRECATION")
            for (network in connectivityManager.allNetworks) {
                val caps = connectivityManager.getNetworkCapabilities(network) ?: continue
                if (!caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) continue
                val wifiInfo = caps.transportInfo as? WifiInfo
                val ssid = wifiInfo?.ssid?.removeSurrounding("\"")
                Log.i(TAG, "Network check: transport=WiFi, ssid=$ssid")
                val vehicle = ssid?.let { ssidMap[it] }
                if (vehicle != null) {
                    Log.i(TAG, "Already connected to vehicle '${vehicle.name}' via ConnectivityManager")
                    wifiNetwork = network
                    activeVehicle = vehicle
                    startCompanionService(vehicle)
                    return
                }
                if (wifiNetwork == null) wifiNetwork = network
            }
        } catch (e: Exception) {
            Log.w(TAG, "ConnectivityManager network scan failed", e)
        }

        // Fallback: WifiManager.connectionInfo
        try {
            val wifiManager = context.getSystemService(WifiManager::class.java)
            @Suppress("DEPRECATION")
            val info = wifiManager.connectionInfo
            val ssid = info?.ssid?.removeSurrounding("\"")
            Log.i(TAG, "WifiManager fallback: ssid=$ssid")
            val vehicle = ssid?.let { ssidMap[it] }
            if (vehicle != null) {
                Log.i(TAG, "Already connected to vehicle '${vehicle.name}' via WifiManager")
                activeVehicle = vehicle
                startCompanionService(vehicle)
                return
            }
        } catch (e: Exception) {
            Log.w(TAG, "WifiManager fallback failed", e)
        }

        Log.i(TAG, "No paired vehicle SSID found in current networks")
    }

    fun stop() {
        if (!registered) return
        connectivityManager.unregisterNetworkCallback(networkCallback)
        registered = false
    }

    fun getWifiNetwork(): Network? = wifiNetwork

    private fun startCompanionService(vehicle: Vehicle) {
        val intent = Intent(context, CompanionService::class.java).apply {
            putExtra("shared_secret", vehicle.sharedSecret)
            putExtra("vehicle_name", vehicle.name)
            putExtra("vehicle_id", vehicle.id)
            putExtra("socks5_enabled", vehicle.socks5Enabled)
        }
        ContextCompat.startForegroundService(context, intent)
    }

    private fun stopCompanionService() {
        context.stopService(Intent(context, CompanionService::class.java))
    }

    companion object {
        private const val TAG = "WifiMonitor"
    }
}
```

**Step 2: Update CompanionApp.startWifiMonitor signature**

In `CompanionApp.kt`, change the method to accept a vehicle list:

```kotlin
fun startWifiMonitor(vehicles: List<Vehicle>) {
    wifiMonitor?.stop()
    wifiMonitor = WifiMonitor(this, vehicles).also { it.start() }
}
```

Add import: `import org.openauto.companion.data.Vehicle`

Remove old `startWifiMonitor(sharedSecret: String, targetSsid: String)` signature.

**Step 3: Update CompanionService to use vehicle name and per-vehicle socks5**

In `CompanionService.kt`, read the new intent extras in `onStartCommand`:

- Line 55: After `val secret = ...`, add:
```kotlin
val vehicleName = intent?.getStringExtra("vehicle_name") ?: "OpenAuto Prodigy"
val socks5EnabledOverride = intent?.getBooleanExtra("socks5_enabled", true) ?: true
```

- Line 69: Change notification text from `"Connected to OpenAuto Prodigy"` to `"Connected to $vehicleName"`

- Line 136-137: In `startSocks5`, replace reading from CompanionPrefs with the intent-provided value. Change the `startSocks5` signature to accept the enabled flag:
```kotlin
private fun startSocks5(secret: String, enabled: Boolean) {
    if (!enabled) {
        Log.i(TAG, "SOCKS5 proxy disabled for this vehicle")
        return
    }
    // ... rest unchanged
}
```

- Call site at line 68: `startSocks5(secret, socks5EnabledOverride)`

Also add a `_vehicleName` StateFlow alongside `_connected` and `_socks5Active`:
```kotlin
private val _vehicleName = MutableStateFlow("")
val vehicleName: StateFlow<String> = _vehicleName.asStateFlow()
```

Set it in `onStartCommand`: `_vehicleName.value = vehicleName`

Clear in `onDestroy`: `_vehicleName.value = ""`

**Step 4: Build to verify compilation**

Run: `cd /home/matt/claude/personal/openautopro/companion-app && ./gradlew assembleDebug`
Expected: BUILD SUCCESSFUL (MainActivity will have compile errors — that's expected, fixed in Task 4)

Note: Task 2 will temporarily break MainActivity compilation because it still calls the old `startWifiMonitor(String, String)`. This is fine — Task 4 fixes it. Run `./gradlew test` to verify non-UI tests still pass.

**Step 5: Commit**

```bash
cd /home/matt/claude/personal/openautopro/companion-app
git add app/src/main/java/org/openauto/companion/service/WifiMonitor.kt \
        app/src/main/java/org/openauto/companion/CompanionApp.kt \
        app/src/main/java/org/openauto/companion/service/CompanionService.kt
git commit -m "feat: WifiMonitor multi-SSID matching and per-vehicle service extras"
```

---

### Task 3: PairingScreen with SSID and name fields

**Files:**
- Modify: `companion-app/app/src/main/java/org/openauto/companion/ui/PairingScreen.kt`

**Step 1: Add SSID and name fields to PairingScreen**

The callback now returns `(ssid, name, pin)` instead of just `(pin)`:

```kotlin
package org.openauto.companion.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp

@Composable
fun PairingScreen(
    suggestedSsid: String = "",
    onPaired: (ssid: String, name: String, pin: String) -> Unit,
    onCancel: (() -> Unit)? = null
) {
    var ssid by remember { mutableStateOf(suggestedSsid) }
    var name by remember { mutableStateOf("") }
    var pin by remember { mutableStateOf("") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text("Add Vehicle", style = MaterialTheme.typography.headlineMedium)
        Spacer(modifier = Modifier.height(16.dp))
        Text("Enter the WiFi SSID and 6-digit PIN shown on your head unit's settings screen.")
        Spacer(modifier = Modifier.height(32.dp))

        OutlinedTextField(
            value = ssid,
            onValueChange = { ssid = it },
            label = { Text("WiFi SSID") },
            placeholder = { Text("e.g. OpenAutoProdigy-A3F2") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth()
        )

        Spacer(modifier = Modifier.height(16.dp))

        OutlinedTextField(
            value = name,
            onValueChange = { name = it },
            label = { Text("Vehicle Name (optional)") },
            placeholder = { Text("e.g. Miata") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth()
        )

        Spacer(modifier = Modifier.height(16.dp))

        OutlinedTextField(
            value = pin,
            onValueChange = { if (it.length <= 6 && it.all { c -> c.isDigit() }) pin = it },
            label = { Text("PIN") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            singleLine = true,
            modifier = Modifier.width(200.dp)
        )

        Spacer(modifier = Modifier.height(24.dp))

        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            if (onCancel != null) {
                TextButton(onClick = onCancel) {
                    Text("Cancel")
                }
            }
            Button(
                onClick = { onPaired(ssid.trim(), name.trim().ifEmpty { ssid.trim() }, pin) },
                enabled = pin.length == 6 && ssid.trim().isNotEmpty()
            ) {
                Text("Pair")
            }
        }
    }
}
```

**Step 2: Commit**

```bash
cd /home/matt/claude/personal/openautopro/companion-app
git add app/src/main/java/org/openauto/companion/ui/PairingScreen.kt
git commit -m "feat: PairingScreen with SSID and vehicle name fields"
```

---

### Task 4: VehicleListScreen and StatusScreen updates

**Files:**
- Create: `companion-app/app/src/main/java/org/openauto/companion/ui/VehicleListScreen.kt`
- Modify: `companion-app/app/src/main/java/org/openauto/companion/ui/StatusScreen.kt`

**Step 1: Create VehicleListScreen**

Create `companion-app/app/src/main/java/org/openauto/companion/ui/VehicleListScreen.kt`:

```kotlin
package org.openauto.companion.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import org.openauto.companion.data.Vehicle

@Composable
fun VehicleListScreen(
    vehicles: List<Vehicle>,
    connectedSsid: String?,
    onVehicleTap: (Vehicle) -> Unit,
    onAddVehicle: () -> Unit,
    onRemoveVehicle: (Vehicle) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp)
    ) {
        Text("OpenAuto Companion", style = MaterialTheme.typography.headlineMedium)
        Spacer(modifier = Modifier.height(24.dp))

        LazyColumn(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            items(vehicles, key = { it.id }) { vehicle ->
                val isConnected = vehicle.ssid == connectedSsid
                VehicleRow(
                    vehicle = vehicle,
                    isConnected = isConnected,
                    onClick = { onVehicleTap(vehicle) },
                    onRemove = { onRemoveVehicle(vehicle) }
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        Button(
            onClick = onAddVehicle,
            modifier = Modifier.align(Alignment.CenterHorizontally),
            enabled = vehicles.size < Vehicle.MAX_VEHICLES
        ) {
            Text("Add Vehicle")
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun VehicleRow(
    vehicle: Vehicle,
    isConnected: Boolean,
    onClick: () -> Unit,
    onRemove: () -> Unit
) {
    var showDeleteDialog by remember { mutableStateOf(false) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
    ) {
        Row(
            modifier = Modifier
                .padding(16.dp)
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Connection indicator
            val color = if (isConnected) Color(0xFF4CAF50) else Color(0xFF9E9E9E)
            Surface(
                shape = MaterialTheme.shapes.small,
                color = color,
                modifier = Modifier.size(12.dp)
            ) {}
            Spacer(modifier = Modifier.width(12.dp))

            Column(modifier = Modifier.weight(1f)) {
                Text(vehicle.name, style = MaterialTheme.typography.titleMedium)
                if (vehicle.name != vehicle.ssid) {
                    Text(vehicle.ssid, style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }

            TextButton(onClick = { showDeleteDialog = true }) {
                Text("Remove", color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodySmall)
            }
        }
    }

    if (showDeleteDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteDialog = false },
            title = { Text("Remove ${vehicle.name}?") },
            text = { Text("This will unpair this vehicle. You can re-pair later with a new PIN.") },
            confirmButton = {
                TextButton(onClick = { showDeleteDialog = false; onRemove() }) {
                    Text("Remove", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteDialog = false }) {
                    Text("Cancel")
                }
            }
        )
    }
}
```

**Step 2: Update StatusScreen to show vehicle name**

In `StatusScreen.kt`, add `vehicleName` parameter and use it in the header:

Change the signature:
```kotlin
@Composable
fun StatusScreen(
    vehicleName: String,
    status: CompanionStatus,
    socks5Enabled: Boolean,
    onSocks5Toggle: (Boolean) -> Unit,
    onUnpair: () -> Unit,
    onBack: () -> Unit
)
```

Change line 32 from:
```kotlin
Text("OpenAuto Companion", style = MaterialTheme.typography.headlineMedium)
```
to:
```kotlin
Row(verticalAlignment = Alignment.CenterVertically) {
    TextButton(onClick = onBack) { Text("\u2190") }
    Spacer(modifier = Modifier.width(8.dp))
    Text(vehicleName, style = MaterialTheme.typography.headlineMedium)
}
```

**Step 3: Commit**

```bash
cd /home/matt/claude/personal/openautopro/companion-app
git add app/src/main/java/org/openauto/companion/ui/VehicleListScreen.kt \
        app/src/main/java/org/openauto/companion/ui/StatusScreen.kt
git commit -m "feat: VehicleListScreen and vehicle-aware StatusScreen"
```

---

### Task 5: MainActivity navigation and wiring

**Files:**
- Modify: `companion-app/app/src/main/java/org/openauto/companion/ui/MainActivity.kt`

**Step 1: Rewrite MainActivity for 3-screen navigation**

Replace `MainActivity.kt` with multi-vehicle navigation:

```kotlin
package org.openauto.companion.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.*
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import org.openauto.companion.CompanionApp
import org.openauto.companion.data.CompanionPrefs
import org.openauto.companion.data.Vehicle
import org.openauto.companion.service.CompanionService
import java.security.MessageDigest

class MainActivity : ComponentActivity() {
    private lateinit var prefs: CompanionPrefs

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        if (results.values.all { it }) {
            startMonitoringIfPaired()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = CompanionPrefs(this)

        requestPermissions()

        setContent {
            MaterialTheme {
                var vehicles by remember { mutableStateOf(prefs.vehicles) }
                var screen by remember { mutableStateOf<Screen>(
                    if (vehicles.isEmpty()) Screen.Pairing else Screen.VehicleList
                ) }

                val isConnected by CompanionService.connected.collectAsStateWithLifecycle()
                val isSocks5Active by CompanionService.socks5Active.collectAsStateWithLifecycle()
                val connectedVehicleName by CompanionService.vehicleName.collectAsStateWithLifecycle()

                // Determine which SSID is currently connected (by matching vehicle name back)
                val connectedSsid = if (isConnected) {
                    vehicles.find { it.name == connectedVehicleName }?.ssid
                } else null

                when (val s = screen) {
                    is Screen.VehicleList -> {
                        VehicleListScreen(
                            vehicles = vehicles,
                            connectedSsid = connectedSsid,
                            onVehicleTap = { screen = Screen.Status(it) },
                            onAddVehicle = { screen = Screen.Pairing },
                            onRemoveVehicle = { vehicle ->
                                prefs.removeVehicle(vehicle.id)
                                vehicles = prefs.vehicles
                                restartMonitoring()
                            }
                        )
                    }
                    is Screen.Pairing -> {
                        PairingScreen(
                            onPaired = { ssid, name, pin ->
                                val secret = deriveSecret(pin)
                                val vehicle = Vehicle(ssid = ssid, name = name, sharedSecret = secret)
                                prefs.addVehicle(vehicle)
                                vehicles = prefs.vehicles
                                restartMonitoring()
                                screen = Screen.VehicleList
                            },
                            onCancel = if (vehicles.isNotEmpty()) {
                                { screen = Screen.VehicleList }
                            } else null
                        )
                    }
                    is Screen.Status -> {
                        val vehicle = s.vehicle
                        val isThisConnected = isConnected && connectedSsid == vehicle.ssid
                        val status = CompanionStatus(
                            connected = isThisConnected,
                            sharingTime = true,
                            sharingGps = true,
                            sharingBattery = true,
                            socks5Active = isThisConnected && isSocks5Active,
                            ssid = vehicle.ssid
                        )
                        var socks5Enabled by remember(vehicle.id) {
                            mutableStateOf(vehicle.socks5Enabled)
                        }

                        StatusScreen(
                            vehicleName = vehicle.name,
                            status = status,
                            socks5Enabled = socks5Enabled,
                            onSocks5Toggle = {
                                socks5Enabled = it
                                prefs.updateVehicle(vehicle.id) { v -> v.copy(socks5Enabled = it) }
                            },
                            onUnpair = {
                                prefs.removeVehicle(vehicle.id)
                                vehicles = prefs.vehicles
                                restartMonitoring()
                                screen = if (vehicles.isEmpty()) Screen.Pairing else Screen.VehicleList
                            },
                            onBack = { screen = Screen.VehicleList }
                        )
                    }
                }
            }
        }
    }

    private fun requestPermissions() {
        val needed = mutableListOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION
        )
        if (Build.VERSION.SDK_INT >= 33) {
            needed.add(Manifest.permission.POST_NOTIFICATIONS)
            needed.add(Manifest.permission.NEARBY_WIFI_DEVICES)
        }

        val missing = needed.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missing.isNotEmpty()) {
            permissionLauncher.launch(missing.toTypedArray())
        } else {
            startMonitoringIfPaired()
        }
    }

    private fun deriveSecret(pin: String): String {
        val material = "$pin:openauto-companion-v1"
        val digest = MessageDigest.getInstance("SHA-256")
        return digest.digest(material.toByteArray()).joinToString("") { "%02x".format(it) }
    }

    private fun startMonitoringIfPaired() {
        if (prefs.isPaired) {
            (application as CompanionApp).startWifiMonitor(prefs.vehicles)
        }
    }

    private fun restartMonitoring() {
        if (prefs.isPaired) {
            (application as CompanionApp).startWifiMonitor(prefs.vehicles)
        }
    }
}

private sealed class Screen {
    data object VehicleList : Screen()
    data object Pairing : Screen()
    data class Status(val vehicle: Vehicle) : Screen()
}
```

**Step 2: Build and run all tests**

Run: `cd /home/matt/claude/personal/openautopro/companion-app && ./gradlew assembleDebug && ./gradlew test`
Expected: BUILD SUCCESSFUL, all tests pass.

**Step 3: Commit**

```bash
cd /home/matt/claude/personal/openautopro/companion-app
git add app/src/main/java/org/openauto/companion/ui/MainActivity.kt
git commit -m "feat: multi-vehicle navigation with VehicleList, Pairing, and Status screens"
```

---

### Task 6: Install script UUID-suffixed SSID default

**Files:**
- Modify: `/home/matt/claude/personal/openautopro/openauto-prodigy/install.sh:159-161`

**Step 1: Update SSID prompt**

Replace lines 159-161 in `install.sh`:

```bash
        # FROM:
        read -p "WiFi hotspot SSID [OpenAutoProdigy]: " WIFI_SSID
        WIFI_SSID=${WIFI_SSID:-OpenAutoProdigy}

        # TO:
        VEHICLE_UUID=$(head -c 2 /dev/urandom | xxd -p | tr '[:lower:]' '[:upper:]')
        DEFAULT_SSID="OpenAutoProdigy-${VEHICLE_UUID}"
        echo ""
        info "The companion app uses the WiFi SSID to identify this vehicle."
        info "The default includes a unique suffix to avoid conflicts with multiple vehicles."
        info "If you enter a custom name, make sure it's unique across your vehicles."
        echo ""
        read -p "WiFi hotspot SSID [$DEFAULT_SSID]: " WIFI_SSID
        WIFI_SSID=${WIFI_SSID:-$DEFAULT_SSID}
```

**Step 2: Commit**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git add install.sh
git commit -m "feat: UUID-suffixed default SSID in installer for multi-vehicle support"
```

---

### Task 7: E2E test — build, deploy, verify

**Step 1: Build companion APK**

Run: `cd /home/matt/claude/personal/openautopro/companion-app && ./gradlew assembleDebug`

**Step 2: Install on phone**

Run: `/home/matt/claude/personal/openautopro/openauto-prodigy/platform-tools/adb install -r /home/matt/claude/personal/openautopro/companion-app/app/build/outputs/apk/debug/app-debug.apk`

**Step 3: Restart and verify**

Force-stop and relaunch:
```bash
ADB=/home/matt/claude/personal/openautopro/openauto-prodigy/platform-tools/adb
$ADB shell am force-stop org.openauto.companion
$ADB shell am start -n org.openauto.companion/.ui.MainActivity
```

Wait ~8 seconds, then:

1. Take screenshot: verify VehicleListScreen shows (migration should have created one vehicle from old prefs)
2. Check SOCKS5 port: `$ADB shell ss -tlnp | grep 1080`
3. Test proxy: `ssh matt@192.168.1.149 "curl -x socks5h://oap:<secret>@10.0.0.26:1080 -m 10 -s http://ifconfig.me"`

**Step 4: Verify migration worked**

```bash
$ADB shell "run-as org.openauto.companion cat shared_prefs/companion.xml"
```

Should show `vehicles_json` key with one vehicle, and NO `shared_secret` / `target_ssid` keys (legacy keys removed by migration).

**Step 5: Push both repos and update companion release**

```bash
# Companion app
cd /home/matt/claude/personal/openautopro/companion-app
git push origin main

# Prodigy (install.sh change)
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git push origin main

# Replace release
cd /home/matt/claude/personal/openautopro/companion-app
gh release delete v0.2.0-alpha --yes
git push origin :refs/tags/v0.2.0-alpha
gh release create v0.2.0-alpha \
  --title "v0.2.0-alpha — Multi-Vehicle + Internet Sharing" \
  --prerelease \
  --notes "Multi-vehicle support and SOCKS5 internet sharing" \
  app/build/outputs/apk/debug/app-debug.apk
```
