// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// Fireface 800 editor preview with a read-only driver-cache panel.
// Editable values are local drafts, NOT current hardware state.
import SwiftUI

struct Fireface800SettingsView: View {
    @ObservedObject var controls: DeviceControlsViewModel

    enum Jack: Int, CaseIterable, Identifiable {
        case rear = 0
        case front = 1
        case both = 2

        var id: Int { rawValue }
        var title: String {
            switch self {
            case .rear: "Rear"
            case .front: "Front"
            case .both: "Front + Rear"
            }
        }
    }

    enum InputLevel: Int, CaseIterable, Identifiable {
        case lowGain = 0
        case plus4 = 1
        case minus10 = 2

        var id: Int { rawValue }
        var title: String {
            switch self {
            case .lowGain: "Low Gain"
            case .plus4: "+4 dBu"
            case .minus10: "-10 dBV"
            }
        }
    }

    enum OutputLevel: Int, CaseIterable, Identifiable {
        case highGain = 0
        case plus4 = 1
        case minus10 = 2

        var id: Int { rawValue }
        var title: String {
            switch self {
            case .highGain: "High Gain"
            case .plus4: "+4 dBu"
            case .minus10: "-10 dBV"
            }
        }
    }

    @State private var phantom7 = false
    @State private var phantom8 = false
    @State private var phantom9 = false
    @State private var phantom10 = false
    @State private var input1: Jack = .front
    @State private var input7: Jack = .front
    @State private var input8: Jack = .front
    @State private var inputLevel: InputLevel = .lowGain
    @State private var outputLevel: OutputLevel = .highGain

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                HStack {
                    Label("Fireface 800", systemImage: "slider.vertical.3")
                        .font(.title2.bold())
                    Spacer()
                    Text("Editor preview")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                }

                Label {
                    Text("These settings are local drafts only. The app does not read the current Fireface 800 hardware settings or send changes to the device from this screen. Do not rely on these switches as an indication of phantom-power state.")
                } icon: {
                    Image(systemName: "exclamationmark.triangle")
                }
                .font(.callout)
                .padding(14)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(.orange.opacity(0.10), in: RoundedRectangle(cornerRadius: 10))

                Label {
                    Text("Saved values from the device can now be inspected under Device Controls after the driver has read the FF800 flash during audio configuration. They are cached settings, not live hardware feedback. This editor still does not apply changes.")
                } icon: {
                    Image(systemName: "info.circle")
                }
                .font(.callout)
                .foregroundStyle(.secondary)

                savedSettingsSection

                GroupBox("Microphone phantom power (48 V)") {
                    VStack(alignment: .leading, spacing: 12) {
                        Toggle("Input 7", isOn: $phantom7)
                        Toggle("Input 8", isOn: $phantom8)
                        Toggle("Input 9", isOn: $phantom9)
                        Toggle("Input 10", isOn: $phantom10)
                        Text("Draft selections only. No voltage is switched.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .padding(8)
                }

                GroupBox("Input sockets") {
                    VStack(alignment: .leading, spacing: 12) {
                        jackPicker("Input 1", selection: $input1)
                        jackPicker("Input 7", selection: $input7)
                        jackPicker("Input 8", selection: $input8)
                    }
                    .padding(8)
                }

                GroupBox("Analog reference levels") {
                    VStack(alignment: .leading, spacing: 12) {
                        HStack {
                            Text("Inputs")
                            Spacer()
                            Picker("Input level", selection: $inputLevel) {
                                ForEach(InputLevel.allCases) { level in
                                    Text(level.title).tag(level)
                                }
                            }
                            .labelsHidden()
                            .frame(width: 160)
                        }
                        HStack {
                            Text("Outputs")
                            Spacer()
                            Picker("Output level", selection: $outputLevel) {
                                ForEach(OutputLevel.allCases) { level in
                                    Text(level.title).tag(level)
                                }
                            }
                            .labelsHidden()
                            .frame(width: 160)
                        }
                    }
                    .padding(8)
                }

                HStack {
                    Button("Reset draft") { resetDraft() }
                    Spacer()
                    Text("Hardware write unavailable")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Button("Apply to Fireface 800") {}
                        .disabled(true)
                }
            }
            .padding(24)
            .frame(maxWidth: 860, alignment: .leading)
            .frame(maxWidth: .infinity, alignment: .top)
        }
        .navigationTitle("Fireface 800")
        .task { refreshSavedSettings() }
    }


    // Treat flash values as a coherent read-only snapshot, never as live
    // measurements. Do not render a snapshot left over from another device,
    // a disconnected driver, or a different control provider.
    private var validatedSavedSnapshot: ControlSnapshot? {
        guard controls.isConnected,
              let selected = controls.selectedDevice,
              let snapshot = controls.snapshot,
              snapshot.family == "rme-ff800-flash",
              snapshot.guid.caseInsensitiveCompare(selected.guidHex) == .orderedSame,
              snapshot.parameters.count == 9,
              Set(snapshot.parameters.map(\.id)) == Set(UInt32(0xF80001)...UInt32(0xF80009)),
              snapshot.parameters.allSatisfy({
                  $0.access == .readOnly && $0.origin == .cached && $0.value != nil
              }) else {
            return nil
        }
        return snapshot
    }

    @ViewBuilder
    private var savedSettingsSection: some View {
        GroupBox("Saved device settings (read only)") {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text("Last flash settings cached by the driver")
                    Spacer()
                    Button("Refresh") { refreshSavedSettings() }
                        .disabled(!controls.isConnected)
                }
                Text("Not live measurements. A saved 48 V setting does NOT confirm that phantom power is currently on or off.")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                if let snapshot = validatedSavedSnapshot {
                    ForEach(snapshot.parameters.sorted(by: { $0.id < $1.id })) { parameter in
                        HStack {
                            Text(parameter.name)
                            Spacer()
                            Text(savedValue(parameter))
                                .monospacedDigit()
                                .foregroundStyle(.secondary)
                        }
                        .font(.callout)
                    }
                } else if !controls.isConnected {
                    Label("Driver not connected", systemImage: "cable.connector.slash")
                        .foregroundStyle(.secondary)
                } else {
                    Text("No valid cached FF800 snapshot available for the selected device. A snapshot must identify this device and contain nine read-only, cached settings. Flash settings are captured during audio configuration; Refresh does not read the hardware.")
                        .font(.callout)
                        .foregroundStyle(.secondary)
                }
            }
            .padding(8)
        }
    }

    private func refreshSavedSettings() {
        // Names from discovery are not guaranteed to contain "Fireface 800".
        // Identify the cached settings by the provider's explicit family tag.
        // The shared model's calls here read only driver snapshots, never
        // device registers. Restore the prior selection if none is available.
        let previousGUID = controls.selectedGUID
        controls.reload()
        if validatedSavedSnapshot != nil { return }
        for device in controls.devices where device.guid != controls.selectedGUID {
            controls.select(device.guid)
            if validatedSavedSnapshot != nil { return }
        }
        if let previousGUID, controls.devices.contains(where: { $0.guid == previousGUID }) {
            controls.select(previousGUID)
        }
    }

    private func savedValue(_ parameter: ControlParameter) -> String {
        guard let value = parameter.value else { return "Unavailable" }
        switch value {
        case .bool(let on): return on ? "Saved: On" : "Saved: Off"
        case .int(let raw):
            if case .enumeration(let items) = parameter.domain,
               let item = items.first(where: { $0.value == raw }) {
                return item.name
            }
            return String(raw)
        case .double(let raw): return String(raw)
        case .action: return "Unavailable"
        }
    }

    private func jackPicker(_ title: String, selection: Binding<Jack>) -> some View {
        HStack {
            Text(title)
            Spacer()
            Picker(title, selection: selection) {
                ForEach(Jack.allCases) { jack in
                    Text(jack.title).tag(jack)
                }
            }
            .labelsHidden()
            .frame(width: 160)
        }
    }

    private func resetDraft() {
        phantom7 = false
        phantom8 = false
        phantom9 = false
        phantom10 = false
        input1 = .front
        input7 = .front
        input8 = .front
        inputLevel = .lowGain
        outputLevel = .highGain
    }
}
