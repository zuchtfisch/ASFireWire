// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// Fireface 800 settings editor preview. This view deliberately makes NO driver
// calls: the FF800 controls are not yet projected through ASFW's control service.
// Values are local drafts, NOT current hardware state.
import SwiftUI

struct Fireface800SettingsView: View {
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
