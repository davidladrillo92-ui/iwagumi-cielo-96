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
            NavigationStack { HomeView(model:model).safeAreaInset(edge:.top,spacing:0) { DemoBanner(showing:model.demo) }.toolbar { Button("Conexión",systemImage:"network") { settings = true }.accessibilityIdentifier("connection.open") } }.tabItem { Label("Luz",systemImage:"lightbulb") }
            NavigationStack { CurvesView(model:model).safeAreaInset(edge:.top,spacing:0) { DemoBanner(showing:model.demo) } }.tabItem { Label("Curvas",systemImage:"chart.xyaxis.line") }
            NavigationStack { CentreView(model:model).safeAreaInset(edge:.top,spacing:0) { DemoBanner(showing:model.demo) } }.tabItem { Label("Centro",systemImage:"flask") }
        }
        .tint(Color(red:0.86,green:0.76,blue:0.65))
        .sheet(isPresented:$settings) { SettingsView(model:model) }
        .alert("CIELO",isPresented:Binding(get:{model.message != nil},set:{if !$0 { model.message = nil }})) { Button("Aceptar") { model.message = nil } } message: { Text(model.message ?? "") }
        .onChange(of:phase) { _,next in model.setForeground(next == .active) }
        .task(id:phase) {
            model.setForeground(phase == .active)
            guard phase == .active else { return }
            await model.refresh()
            while !Task.isCancelled {
                do { try await Task.sleep(for:.seconds(6)) } catch { break }
                guard !Task.isCancelled else { break }; if !model.demo { await model.refresh() }
            }
        }
    }
}

struct DemoBanner: View {
    let showing:Bool
    var body:some View {
        if showing { Text("Modo demostración").font(.caption).frame(maxWidth:.infinity).padding(.horizontal,16).padding(.vertical,8).background(.orange.opacity(0.15)).background(Color(uiColor:.systemBackground)).accessibilityLabel("Modo demostración. No controla las luces.") }
    }
}

struct CieloSectionTitle:View {
    let title:String
    var body:some View { Text(title).font(.headline).accessibilityAddTraits(.isHeader) }
}

struct HomeView: View {
    @ObservedObject var model: CieloController
    @State private var confirmStart = false
    @Environment(\.dynamicTypeSize) private var typeSize
    var body: some View {
        List {
            Section {
                VStack(alignment:.leading,spacing:14) {
                    Image(systemName:"mountain.2.fill").font(.system(size:72)).foregroundStyle(.mint.opacity(0.5)).frame(maxWidth:.infinity).padding(.vertical,18).accessibilityHidden(true)
                    Text(model.status?.clock ?? "Conecta tu ATOM").font(.title3)
                    Label(model.fresh ? "ATOM accesible" : "Sin comunicación reciente",systemImage:model.fresh ? "circle.fill" : "circle.dotted").font(.caption).foregroundStyle(model.fresh ? Color.mint : Color.secondary)
                }.padding(.vertical,8)
            }
            if let error = model.connectionError { Section { Text(error).font(.footnote).foregroundStyle(.secondary) } }
            Section {
                CieloSectionTitle(title:"Luces")
                LampRow(model:model,name:"Chihiros",pv:false)
                LampRow(model:model,name:"Pez Verde",pv:true)
            }
            Section {
                CieloSectionTitle(title:"Mantenimiento")
                Text(maintenanceLabel).foregroundStyle(.secondary)
                if let remaining = model.status?.work_light?.remaining_sec, remaining > 0 { Text("Transición: \(remaining / 60) min \(remaining % 60) s").monospacedDigit() }
                actionLayout {
                    startButton.frame(maxWidth:.infinity,alignment:.leading)
                    stopButton.frame(maxWidth:.infinity,alignment:typeSize.isAccessibilitySize ? .leading : .trailing)
                }
            }
            Section { Text("Los valores son consignas. La comunicación BLE no mide la luz física. Las curvas continúan en el ATOM al cerrar la app.").font(.footnote).foregroundStyle(.secondary) }
        }.navigationTitle("CIELO").refreshable { await model.refresh() }
        .confirmationDialog("Iniciar luz de mantenimiento",isPresented:$confirmStart,titleVisibility:.visible) {
            Button("Iniciar") { Task { await model.maintenance(start:true) } }.accessibilityIdentifier("maintenance.confirm")
        } message: { Text(model.demo ? "Solo cambiará la demostración." : "Subida de 10 minutos. Al parar, retorno de 2 minutos a la programación.") }
    }
    private var actionLayout:AnyLayout { typeSize.isAccessibilitySize ? AnyLayout(VStackLayout(alignment:.leading,spacing:8)) : AnyLayout(HStackLayout(spacing:8)) }
    private var canStart:Bool { model.fresh && !model.busy && model.status?.work_light?.can_start == true }
    private var canStop:Bool { model.fresh && !model.busy && (model.status?.work_light?.active == true || model.status?.work_light?.preparing == true) && model.status?.work_light?.phase != "returning" }
    private var startButton:some View {
        Button { confirmStart = true } label: { HStack { Image(systemName:"play.fill").font(.body).accessibilityHidden(true); Text("Iniciar").font(.body) }.frame(minWidth:44,minHeight:44).contentShape(Rectangle()) }
            .buttonStyle(.borderless).accessibilityIdentifier("maintenance.start").disabled(!canStart).opacity(canStart ? 1 : 0.4)
    }
    private var stopButton:some View {
        Button { Task { await model.maintenance(start:false) } } label: { HStack { Image(systemName:"stop.fill").font(.body).accessibilityHidden(true); Text("Parar").font(.body) }.frame(minWidth:44,minHeight:44).contentShape(Rectangle()) }
            .buttonStyle(.borderless).accessibilityIdentifier("maintenance.stop").disabled(!canStop).opacity(canStop ? 1 : 0.4)
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
    @Environment(\.dynamicTypeSize) private var typeSize
    var body: some View {
        rowLayout {
            details.frame(maxWidth:.infinity,alignment:.leading)
            Text(percentLabel).font(.title2).monospacedDigit().frame(maxWidth:typeSize.isAccessibilitySize ? .infinity : nil,alignment:.leading)
        }.padding(.vertical,7)
    }
    private var rowLayout:AnyLayout { typeSize.isAccessibilitySize ? AnyLayout(VStackLayout(alignment:.leading,spacing:10)) : AnyLayout(HStackLayout(spacing:12)) }
    private var details:some View {
        VStack(alignment:.leading,spacing:5) {
            Text(name).font(.headline)
            Text(label).font(.caption).foregroundStyle(.secondary)
            Text(model.fresh && values.count == 3 ? "R \(values[0]) · G \(values[1]) · B \(values[2])" : "RGB sin confirmar").font(.caption).foregroundStyle(.secondary)
        }
    }
    private var presentation:LampPresentation {
        model.status?.cieloV4?.presentation(pv:pv,fresh:model.fresh) ?? LampPresentation(label:model.fresh ? "Comprobando conexión" : "Sin comunicación",values:nil)
    }
    private var percentLabel:String {
        guard let values=presentation.values else { return "—" }
        return "\(pv ? values[3] : values.max() ?? 0)%"
    }
    private var values:[Int] { Array((presentation.values ?? []).prefix(3)) }
    private var label:String { presentation.label }

}

struct PlotPoint: Identifiable { var id:String; var minute:Int; var value:Int; var light:String }
struct PointChoice: Identifiable { var id = UUID(); var pv:Bool; var index:Int; var row:[Int]; var checksum:String; var limits:PointLimits }
struct CurvesView: View {
    @ObservedObject var model:CieloController
    @State private var pv = false
    @Environment(\.dynamicTypeSize) private var typeSize
    @ScaledMetric(relativeTo:.body) private var chartHeight:CGFloat = 220
    @State private var editor:PointChoice?
    @State private var importing = false
    @State private var exporting = false
    @State private var document = ProfileDocument(data:Data())
    @State private var confirmSave = false
    @State private var previewMinute = 900.0
    @State private var pointsExpanded = false
    var body: some View {
        List {
            Section {
                Chart(plot) { p in
                    LineMark(x:.value("Hora",p.minute),y:.value("Consigna",p.value)).foregroundStyle(by:.value("Luz",p.light)).symbol(.circle)
                }.chartForegroundStyleScale(["Pez Verde":Color(red:0.89,green:0.65,blue:0.55),"Chihiros":Color(red:0.77,green:0.87,blue:0.81)]).chartYScale(domain:0...100).chartXScale(domain:0...1440)
                    .chartXAxis { AxisMarks(values:typeSize.isAccessibilitySize ? [0,1440] : [0,360,720,1080,1440]) { v in AxisGridLine(); AxisValueLabel { if let t = v.as(Int.self) { Text(timeLabel(t)) } } } }
                    .frame(height:chartHeight).accessibilityLabel("Curvas programadas de ambas luces")
                Text("Valores programados · no cambian las luces hasta Guardar").font(.caption).foregroundStyle(.secondary)
            }
            Section {
                HStack { Text(timeLabel(Int(previewMinute))).font(.title2).monospacedDigit().accessibilityIdentifier("preview.time"); Spacer(); Text("Vista previa").font(.caption).foregroundStyle(.secondary) }
                Slider(value:$previewMinute,in:0...1440,step:1).accessibilityLabel("Hora de vista previa").accessibilityIdentifier("preview.slider")
                HStack {
                    Button { previewMinute = 0 } label: { Text("00:00").font(.caption).monospacedDigit().frame(minWidth:44,minHeight:44) }.buttonStyle(.borderless).accessibilityLabel("Ver medianoche").accessibilityIdentifier("preview.start")
                    Spacer()
                    Button { previewMinute = 1440 } label: { Text("24:00").font(.caption).monospacedDigit().frame(minWidth:44,minHeight:44) }.buttonStyle(.borderless).accessibilityLabel("Ver final del día").accessibilityIdentifier("preview.end")
                }
                if let profile = model.profile {
                    PreviewLampRow(name:"Chihiros",values:profile.preview(minute:Int(previewMinute),pv:false),pv:false)
                    PreviewLampRow(name:"Pez Verde",values:profile.preview(minute:Int(previewMinute),pv:true),pv:true)
                }
            } footer: { Text("La vista previa no cambia las luces.") }
            Section {
                Button("Guardar ambas curvas") { confirmSave = true }.disabled(!model.canSave).accessibilityIdentifier("curves.save")
                Button("Descartar borrador",role:.destructive) { Task { await model.discard() } }.disabled(!model.dirty || model.busy).accessibilityIdentifier("curves.discard")
                Button("Importar JSON") { importing = true }.disabled(model.busy || model.profile == nil).accessibilityIdentifier("curves.import")
                Button("Exportar JSON") { exportProfile() }.disabled(model.profile == nil || model.busy).accessibilityIdentifier("curves.export")
                if model.draftConflict { Text("El ATOM tiene otro perfil. Exporta tu borrador o descártalo para cargar el actual.").font(.footnote).foregroundStyle(.orange) }
                if model.dirty { Text("Cambios sin guardar").foregroundStyle(.orange).accessibilityIdentifier("curves.dirty") }
            }
            Section {
                Button {
                    withAnimation { pointsExpanded.toggle() }
                } label: {
                    HStack { Text("Editar puntos"); Spacer(); Image(systemName:pointsExpanded ? "chevron.up" : "chevron.down").accessibilityHidden(true) }.contentShape(Rectangle())
                }.accessibilityIdentifier("curves.points").accessibilityValue(pointsExpanded ? "Desplegado" : "Contraído")
                if pointsExpanded {
                    Picker("Luz",selection:$pv) { Text("Chihiros").tag(false); Text("Pez Verde").tag(true) }.pickerStyle(.segmented)
                    ForEach(Array(rows.enumerated()),id:\.offset) { i,row in
                        Button {
                            guard let profile=model.profile, let limits=profile.editLimits(pv:pv,index:i) else { return }
                            editor = PointChoice(pv:pv,index:i,row:row,checksum:profile.computedChecksum,limits:limits)
                        } label: {
                            HStack { Text(timeLabel(row[0])).monospacedDigit(); Spacer(); Text(row.dropFirst().map(String.init).joined(separator:" / ")).foregroundStyle(.secondary); Image(systemName:"chevron.right").font(.caption).accessibilityHidden(true) }
                        }.disabled(model.busy).accessibilityIdentifier("curve.point.\(i)")
                    }
                }
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
        .confirmationDialog(model.demo ? "Guardar curvas de demostración" : "Guardar ambas curvas en el ATOM",isPresented:$confirmSave,titleVisibility:.visible) {
            Button("Guardar") { Task { await model.save() } }.accessibilityIdentifier("curves.confirm")
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

struct PreviewLampRow: View {
    let name:String; let values:[Int]; let pv:Bool
    @Environment(\.dynamicTypeSize) private var typeSize
    private var percent:Int { pv ? values[3] : values.prefix(3).max() ?? 0 }
    private var color:Color {
        let divisor = pv ? 255.0 : 100.0
        let scale = pv ? Double(percent)/100.0 : 1.0
        return Color(red:Double(values[0])/divisor*scale,green:Double(values[1])/divisor*scale,blue:Double(values[2])/divisor*scale)
    }
    private var rowLayout:AnyLayout { typeSize.isAccessibilitySize ? AnyLayout(VStackLayout(alignment:.leading,spacing:8)) : AnyLayout(HStackLayout(spacing:12)) }
    var body:some View {
        rowLayout {
            HStack(spacing:12) {
                RoundedRectangle(cornerRadius:9).fill(color).frame(width:38,height:38).overlay(RoundedRectangle(cornerRadius:9).stroke(.white.opacity(0.15))).accessibilityHidden(true)
                VStack(alignment:.leading,spacing:4) { Text(name).font(.headline); Text("R \(values[0]) · G \(values[1]) · B \(values[2])").font(.caption).foregroundStyle(.secondary) }
            }.frame(maxWidth:.infinity,alignment:.leading)
            Text("\(percent)%").font(.title3).monospacedDigit().frame(maxWidth:typeSize.isAccessibilitySize ? .infinity : nil,alignment:.leading)
        }.padding(.vertical,4)
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
                Stepper("Hora: \(timeLabel(row[0]))",value:$row[0],in:choice.limits.minute).disabled(choice.limits.minute.lowerBound == choice.limits.minute.upperBound).accessibilityIdentifier("point.time")
                ForEach(1...3,id:\.self) { i in
                    Stepper("\(["","R","G","B"][i]): \(row[i])",value:$row[i],in:0...(choice.pv ? 255 : 100)).disabled(choice.limits.channelsLocked).accessibilityIdentifier("point.channel.\(i)")
                }
                if choice.pv { Stepper("Brillo: \(row[4])%",value:$row[4],in:0...100).disabled(choice.limits.brightnessLocked) }
                Text("La hora respeta los puntos vecinos y el apagado final.").font(.caption).foregroundStyle(.secondary)
            }.navigationTitle("Editar punto")
                .toolbar { ToolbarItem(placement:.cancellationAction) { Button("Cancelar") { dismiss() } }; ToolbarItem(placement:.confirmationAction) { Button("Aplicar") { apply(row);dismiss() }.accessibilityIdentifier("point.apply") } }
        }
    }
}

struct CentreView:View {
    @ObservedObject var model:CieloController
    @Environment(\.dynamicTypeSize) private var typeSize
    var body:some View {
        List {
            Section {
                CieloSectionTitle(title:"Estado")
                Text(model.fresh ? (model.status?.health?.current_critical == true ? "Revisar CIELO" : "Datos recientes del ATOM") : "Sin comunicación reciente")
                LampRow(model:model,name:"Chihiros",pv:false); LampRow(model:model,name:"Pez Verde",pv:true)
                LabeledContent("Firmware",value:model.status?.firmware ?? "Sin confirmar")
                LabeledContent("Arranque",value:model.status?.boot_id.map(String.init) ?? "—")
            }
            Section {
                CieloSectionTitle(title:"Informe")
                Button("Preparar informe para compartir") { Task { await model.downloadReport() } }.accessibilityIdentifier("report.prepare").disabled(!model.fresh || model.busy)
                if let url=model.reportURL { ShareLink("Compartir informe",item:url).accessibilityIdentifier("report.share") }
            }
            Section {
                CieloSectionTitle(title:"Privacidad")
                Text("CIELO se comunica con tu ATOM por la red local. No crea cuentas ni incorpora publicidad o analítica. Los perfiles e informes solo se comparten cuando tú lo eliges.")
                Text("Telegram depende del vigilante externo. La app no vigila las luces cuando está cerrada.")
            }
            .font(.footnote).foregroundStyle(.secondary)
        }.navigationTitle("Centro").refreshable { await model.refresh() }
    }
}

struct SettingsView:View {
    @ObservedObject var model:CieloController
    @Environment(\.dismiss) private var dismiss
    @State private var host = ""
    @State private var demo = true
    @State private var confirm = false
    @State private var localError: String?
    @MainActor init(model:CieloController) {
        self.model=model
        _host=State(initialValue:model.host)
        _demo=State(initialValue:model.demo)
    }
    var body:some View {
        NavigationStack {
            Form {
                Toggle("Modo demostración",isOn:$demo).accessibilityIdentifier("connection.demo")
                TextField("IP del ATOM",text:$host).keyboardType(.numbersAndPunctuation).textInputAutocapitalization(.never).autocorrectionDisabled().disabled(demo).accessibilityIdentifier("connection.host")
                Text("Para controlar tus luces, conecta el iPhone a la misma red que el ATOM. La IP se usa solo durante esta sesión.").font(.footnote).foregroundStyle(.secondary)
                if let error = localError { Text(error).font(.footnote).foregroundStyle(.orange).accessibilityIdentifier("connection.error") }
                Button("Aplicar conexión") {
                    do { if !demo { _ = try LocalAddress.url(host) }; localError = nil; confirm = true }
                    catch { localError = error.localizedDescription }
                }.accessibilityIdentifier("connection.apply").disabled(model.busy)
            }.navigationTitle("Conexión")
                .toolbar { Button("Cerrar") { dismiss() } }
                .confirmationDialog(model.dirty ? "Cambiar la conexión descartará el borrador" : "Usar esta conexión",isPresented:$confirm,titleVisibility:.visible) {
                    Button("Continuar",role:model.dirty ? .destructive : nil) { Task { await model.connect(host:host,demo:demo);dismiss() } }
                }
        }
    }
}
