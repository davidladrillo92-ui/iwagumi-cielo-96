import SwiftUI
import Charts
import UniformTypeIdentifiers

struct ProfileDocument: FileDocument {
    static var readableContentTypes: [UTType] { [.json] }
    var data: Data
    init(data: Data) { self.data = data }
    init(configuration: ReadConfiguration) throws { data = configuration.file.regularFileContents ?? Data() }
    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper { FileWrapper(regularFileWithContents:data) }
}

struct RootView: View {
    @StateObject private var model = CieloController()
    @Environment(\.scenePhase) private var phase
    @State private var settings = false
    var body: some View {
        TabView {
            NavigationStack { HomeView(model:model).toolbar { Button("Conexión",systemImage:"network") { settings = true } } }.tabItem { Label("Luz",systemImage:"lightbulb") }
            NavigationStack { CurvesView(model:model) }.tabItem { Label("Curvas",systemImage:"chart.xyaxis.line") }
            NavigationStack { CentreView(model:model) }.tabItem { Label("Centro",systemImage:"flask") }
        }
        .tint(Color(red:0.86,green:0.76,blue:0.65))
        .safeAreaInset(edge:.top,spacing:0) {
            if model.demo { Text("DEMOSTRACIÓN · no controla las luces").font(.caption).frame(maxWidth:.infinity).padding(7).background(.orange.opacity(0.15)) }
        }
        .sheet(isPresented:$settings) { SettingsView(model:model) }
        .alert("CIELO",isPresented:Binding(get:{model.message != nil},set:{if !$0 { model.message = nil }})) { Button("Aceptar") { model.message = nil } } message: { Text(model.message ?? "") }
        .onChange(of:phase) { _,_ in model.invalidate() }
        .task(id:phase) {
            guard phase == .active else { model.invalidate(); return }
            await model.refresh()
            while !Task.isCancelled {
                do { try await Task.sleep(for:.seconds(6)) } catch { break }
                guard !Task.isCancelled else { break }; await model.refresh()
            }
        }
    }
}

struct HomeView: View {
    @ObservedObject var model: CieloController
    @State private var confirmStart = false
    var body: some View {
        List {
            Section {
                VStack(alignment:.leading,spacing:14) {
                    Image(systemName:"mountain.2.fill").font(.system(size:72)).foregroundStyle(.mint.opacity(0.5)).frame(maxWidth:.infinity).padding(.vertical,18)
                    Text(model.status?.clock ?? "Conecta tu ATOM").font(.title3)
                    Label(model.fresh ? "ATOM accesible" : "Sin comunicación reciente",systemImage:model.fresh ? "circle.fill" : "circle.dotted").font(.caption).foregroundStyle(model.fresh ? Color.mint : Color.secondary)
                }.padding(.vertical,8)
            }
            if let error = model.connectionError { Section { Text(error).font(.footnote).foregroundStyle(.secondary) } }
            Section("Luces") {
                LampRow(model:model,name:"Chihiros",pv:false)
                LampRow(model:model,name:"Pez Verde",pv:true)
            }
            Section("Mantenimiento") {
                Text(maintenanceLabel).foregroundStyle(.secondary)
                if let remaining = model.status?.work_light?.remaining_sec, remaining > 0 { Text("Transición: \(remaining / 60) min \(remaining % 60) s").monospacedDigit() }
                HStack {
                    Button("Iniciar",systemImage:"play.fill") { confirmStart = true }
                        .disabled(!model.fresh || model.busy || model.status?.work_light?.can_start != true)
                    Spacer()
                    Button("Parar",systemImage:"stop.fill") { Task { await model.maintenance(start:false) } }
                        .disabled(!model.fresh || model.busy || (model.status?.work_light?.active != true && model.status?.work_light?.preparing != true) || model.status?.work_light?.phase == "returning")
                }.buttonStyle(.borderless)
            }
            Section { Text("Los valores son consignas. La comunicación BLE no mide la luz física. Las curvas continúan en el ATOM al cerrar la app.").font(.footnote).foregroundStyle(.secondary) }
        }.navigationTitle("CIELO").refreshable { await model.refresh() }
        .confirmationDialog("Iniciar luz de mantenimiento",isPresented:$confirmStart,titleVisibility:.visible) {
            Button("Iniciar") { Task { await model.maintenance(start:true) } }
        } message: { Text(model.demo ? "Solo cambiará la demostración." : "Subida de 10 minutos. Al parar, retorno de 2 minutos a la programación.") }
    }
    private var maintenanceLabel: String {
        guard model.fresh else { return "Sin confirmar" }
        let w = model.status?.work_light
        if w?.preparing == true { return "Comprobando ambas luces…" }
        switch w?.phase { case "rising": return "Subiendo"; case "holding": return "Activa"; case "returning": return "Volviendo a la programación"; default: return w?.can_start == true ? "Preparada" : w?.unavailable_reason ?? "Esperando al ATOM" }
    }
}

struct LampRow: View {
    @ObservedObject var model: CieloController
    let name: String; let pv: Bool
    var body: some View {
        HStack {
            VStack(alignment:.leading,spacing:5) {
                Text(name).font(.headline)
                Text(label).font(.caption).foregroundStyle(.secondary)
                Text(model.fresh && values.count == 3 ? "R \(values[0]) · G \(values[1]) · B \(values[2])" : "RGB sin confirmar").font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Text(percentLabel).font(.title2).monospacedDigit()
        }.padding(.vertical,7)
    }
    private var percentLabel: String {
        guard model.fresh else { return "—" }
        if pv { return (model.status?.effectivePv?.last ?? model.status?.brightness).map { "\($0)%" } ?? "—" }
        return values.count == 3 ? "\(values.max() ?? 0)%" : "—"
    }
    private var values: [Int] { (pv ? (model.status?.effectivePv.map { Array($0.prefix(3)) } ?? model.status?.rgb) : model.status?.chLiveRgb) ?? [] }
    private var label: String {
        guard model.fresh else { return "Sin confirmar" }
        let m = model.status?.cieloV4
        let errors = pv ? m?.pv_consecutive_errors : m?.ch_consecutive_errors
        if (errors ?? 0) > 0 || (pv && (m?.pv_off_failures ?? 0) > 0) { return "Revisar comunicación" }
        let connected = pv ? m?.pv_link_connected : m?.ch_link_connected
        if connected == false { return "Sin enlace BLE" }
        if pv && m?.pv_waiting_off_check == true { return "Conectado · en reposo" }
        if (pv ? m?.pv_online_recent : m?.ch_online_recent) == true { return values.allSatisfy({ $0 == 0 }) || (pv && model.status?.brightness == 0) ? "Conectado · en reposo" : "Conectado" }
        return "Comprobando comunicación"
    }
}

struct PlotPoint: Identifiable { var id:String; var minute:Int; var value:Int; var light:String }
struct PointChoice: Identifiable { var id = UUID(); var pv:Bool; var index:Int; var row:[Int]; var checksum:String }
struct CurvesView: View {
    @ObservedObject var model:CieloController
    @State private var pv = false
    @State private var editor:PointChoice?
    @State private var importing = false
    @State private var exporting = false
    @State private var document = ProfileDocument(data:Data())
    @State private var confirmSave = false
    var body: some View {
        List {
            Section {
                Chart(plot) { p in
                    LineMark(x:.value("Hora",p.minute),y:.value("Consigna",p.value)).foregroundStyle(by:.value("Luz",p.light)).symbol(.circle)
                }.chartYScale(domain:0...100).chartXScale(domain:0...1440)
                    .chartXAxis { AxisMarks(values:[0,360,720,1080,1440]) { v in AxisGridLine(); AxisValueLabel { if let t = v.as(Int.self) { Text(timeLabel(t)) } } } }
                    .frame(height:220).accessibilityLabel("Curvas programadas de ambas luces")
                Text("Valores programados · no cambian las luces hasta Guardar").font(.caption).foregroundStyle(.secondary)
            }
            Section {
                Picker("Luz",selection:$pv) { Text("Chihiros").tag(false); Text("Pez Verde").tag(true) }.pickerStyle(.segmented)
                ForEach(Array(rows.enumerated()),id:\.offset) { i,row in
                    Button { editor = PointChoice(pv:pv,index:i,row:row,checksum:model.profile?.computedChecksum ?? "") } label: {
                        HStack { Text(timeLabel(row[0])).monospacedDigit(); Spacer(); Text(row.dropFirst().map(String.init).joined(separator:" / ")).foregroundStyle(.secondary); Image(systemName:"chevron.right").font(.caption) }
                    }
                }
            } header: { Text("Puntos editables") }
            Section {
                Button("Guardar ambas curvas") { confirmSave = true }.disabled(!model.canSave)
                Button("Descartar borrador",role:.destructive) { model.discard() }.disabled(!model.dirty || model.busy)
                Button("Importar JSON") { importing = true }.disabled(model.busy || model.profile == nil)
                Button("Exportar JSON") { exportProfile() }.disabled(model.profile == nil || model.busy)
                if model.dirty { Text("Cambios sin guardar").foregroundStyle(.orange) }
            }
        }.navigationTitle("Curvas").refreshable { await model.refresh() }
        .sheet(item:$editor) { choice in
            PointEditor(choice:choice) { row in
                guard var p = model.profile, p.computedChecksum == choice.checksum else { model.message = CieloError.changed.localizedDescription; return }
                if choice.pv { p.lights.pezverde.points[choice.index] = row }
                else { p.lights.chihiros.points[choice.index] = row }
                p.seal(); model.replaceProfile(p)
            }
        }
        .confirmationDialog("Guardar ambas curvas en el ATOM",isPresented:$confirmSave,titleVisibility:.visible) {
            Button("Guardar") { Task { await model.save() } }
        }
        .fileImporter(isPresented:$importing,allowedContentTypes:[.json]) { result in
            do {
                let url = try result.get(); let scoped = url.startAccessingSecurityScopedResource(); defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                let data = try Data(contentsOf:url); guard data.count <= 131072 else { throw CieloError.invalidProfile }
                model.replaceProfile(try JSONDecoder().decode(CieloProfile.self,from:data))
            } catch { model.message = error.localizedDescription }
        }
        .fileExporter(isPresented:$exporting,document:document,contentType:.json,defaultFilename:"CIELO_PERFIL") { result in if case .failure(let e) = result { model.message = e.localizedDescription } }
    }
    private var rows:[[Int]] { (pv ? model.profile?.lights.pezverde.points : model.profile?.lights.chihiros.points) ?? [] }
    private var plot:[PlotPoint] {
        guard let p = model.profile else { return [] }
        return p.lights.pezverde.points.enumerated().map { PlotPoint(id:"pv\($0.offset)",minute:$0.element[0],value:$0.element[4],light:"Pez Verde") } + p.lights.chihiros.points.enumerated().map { PlotPoint(id:"ch\($0.offset)",minute:$0.element[0],value:$0.element.dropFirst().max() ?? 0,light:"Chihiros") }
    }
    private func exportProfile() {
        do { guard var p = model.profile else { return }; p.seal(); try p.validate(); let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted,.sortedKeys]; document = ProfileDocument(data:try encoder.encode(p)); exporting = true }
        catch { model.message = error.localizedDescription }
    }
}

func timeLabel(_ t:Int) -> String { String(format:"%02d:%02d",t/60,t%60) }
struct PointEditor:View {
    let choice:PointChoice; let apply:([Int])->Void
    @State private var row:[Int]
    @Environment(\.dismiss) private var dismiss
    init(choice:PointChoice,apply:@escaping([Int])->Void) { self.choice=choice;self.apply=apply;_row=State(initialValue:choice.row) }
    var body:some View {
        NavigationStack {
            Form {
                Stepper("Hora: \(timeLabel(row[0]))",value:$row[0],in:0...1440)
                ForEach(1...3,id:\.self) { i in
                    Stepper("\(["","R","G","B"][i]): \(row[i])",value:$row[i],in:0...(choice.pv ? 255 : 100))
                }
                if choice.pv { Stepper("Brillo: \(row[4])%",value:$row[4],in:0...100) }
                Text("La hora debe quedar entre los puntos vecinos. Los extremos deben conservar el apagado.").font(.caption).foregroundStyle(.secondary)
            }.navigationTitle("Editar punto")
                .toolbar { ToolbarItem(placement:.cancellationAction) { Button("Cancelar") { dismiss() } }; ToolbarItem(placement:.confirmationAction) { Button("Aplicar") { apply(row);dismiss() } } }
        }
    }
}

struct CentreView:View {
    @ObservedObject var model:CieloController
    var body:some View {
        List {
            Section("Estado") {
                Text(model.fresh ? (model.status?.health?.current_critical == true ? "Revisar CIELO" : "Datos recientes del ATOM") : "Sin comunicación reciente")
                LampRow(model:model,name:"Chihiros",pv:false); LampRow(model:model,name:"Pez Verde",pv:true)
                LabeledContent("Firmware",value:model.status?.firmware ?? "Sin confirmar")
                LabeledContent("Arranque",value:model.status?.boot_id.map(String.init) ?? "—")
            }
            Section("Informe") {
                Button("Preparar informe para compartir") { Task { await model.downloadReport() } }.disabled(!model.fresh || model.busy)
                if let url=model.reportURL { ShareLink("Compartir informe",item:url) }
            }
            Section("Privacidad") {
                Text("CIELO se comunica con tu ATOM por la red local. No crea cuentas ni incorpora publicidad o analítica. Los perfiles e informes solo se comparten cuando tú lo eliges.")
                Text("Telegram depende del vigilante externo. La app no vigila las luces cuando está cerrada.")
            }.font(.footnote).foregroundStyle(.secondary)
        }.navigationTitle("Centro").refreshable { await model.refresh() }
    }
}

struct SettingsView:View {
    @ObservedObject var model:CieloController
    @Environment(\.dismiss) private var dismiss
    @State private var host = ""
    @State private var demo = true
    @State private var confirm = false
    var body:some View {
        NavigationStack {
            Form {
                Toggle("Modo demostración",isOn:$demo)
                TextField("IP del ATOM",text:$host).keyboardType(.decimalPad).textInputAutocapitalization(.never).autocorrectionDisabled().disabled(demo)
                Text("Para controlar tus luces, conecta el iPhone a la misma red que el ATOM. La IP se usa solo durante esta sesión.").font(.footnote).foregroundStyle(.secondary)
                Button("Aplicar conexión") { confirm = true }.disabled(model.busy)
            }.navigationTitle("Conexión").onAppear { host=model.host;demo=model.demo }
                .toolbar { Button("Cerrar") { dismiss() } }
                .confirmationDialog(model.dirty ? "Cambiar la conexión descartará el borrador" : "Usar esta conexión",isPresented:$confirm,titleVisibility:.visible) {
                    Button("Continuar",role:model.dirty ? .destructive : nil) { Task { await model.connect(host:host,demo:demo);dismiss() } }
                }
        }
    }
}
