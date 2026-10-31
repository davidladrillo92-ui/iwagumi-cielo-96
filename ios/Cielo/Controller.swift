import Foundation
import SwiftUI

final class NoRedirects: NSObject, URLSessionTaskDelegate {
    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse, newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) { completionHandler(nil) }
}

@MainActor final class CieloController: ObservableObject {
    @Published var host = "192.168.0.25"
    @Published var demo = true
    @Published var status: AtomStatus?
    @Published var profile: CieloProfile?
    @Published var dirty = false
    @Published var busy = false
    @Published var message: String?
    @Published var updated: Date?
    @Published var reportURL: URL?
    private var generation: Int?
    private var draftBoot: Int?
    private var revision = UUID()
    private var loading = false
    private var demoPhase = "idle"
    private var demoSaved: CieloProfile?
    private let delegate = NoRedirects()
    private lazy var session: URLSession = {
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = 8; config.timeoutIntervalForResource = 15
        config.urlCache = nil; config.httpShouldSetCookies = false
        config.connectionProxyDictionary = [:]
        return URLSession(configuration: config, delegate: delegate, delegateQueue: nil)
    }()

    var fresh: Bool { updated.map { Date().timeIntervalSince($0) < 20 } ?? false }
    var canSave: Bool { fresh && dirty && !busy && status?.work_light?.active != true && status?.work_light?.preparing != true && generation != nil }
    func invalidate() { updated = nil }
    func connect(host: String, demo: Bool) async {
        guard !busy else { return }
        do { if !demo { _ = try LocalAddress.url(host) } }
        catch { message = error.localizedDescription; return }
        revision = UUID(); self.host = host; self.demo = demo
        status = nil; profile = nil; dirty = false; generation = nil; draftBoot = nil; updated = nil; reportURL = nil
        loading = false
        await refresh()
    }
    private func request(_ path: String, form: [String:String]? = nil) async throws -> Data {
        let base = try LocalAddress.url(host)
        guard let url = URL(string: path, relativeTo: base) else { throw CieloError.invalidHost }
        var request = URLRequest(url: url); request.cachePolicy = .reloadIgnoringLocalCacheData
        if let form {
            request.httpMethod = "POST"
            request.setValue(base.absoluteString, forHTTPHeaderField: "Origin")
            request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
            var parts = URLComponents(); parts.queryItems = form.sorted { $0.key < $1.key }.map { URLQueryItem(name:$0.key,value:$0.value) }
            request.httpBody = parts.percentEncodedQuery?.replacingOccurrences(of: "+", with: "%2B").data(using: .utf8)
        }
        let (data, response) = try await session.data(for: request)
        guard let response = response as? HTTPURLResponse, data.count <= 131072 else { throw CieloError.unavailable }
        guard (200...299).contains(response.statusCode) else {
            if response.statusCode == 409 { throw CieloError.changed }
            let obj = try? JSONSerialization.jsonObject(with: data) as? [String:Any]
            throw CieloError.server(obj?["error"] as? String ?? "El ATOM ha rechazado la operación.")
        }
        return data
    }
    private func demoStatus() throws -> AtomStatus {
        let data = try Data(contentsOf: Bundle.main.url(forResource:"DemoProfile",withExtension:"json")!)
        let p = try demoSaved ?? JSONDecoder().decode(CieloProfile.self,from:data); try p.validate()
        let obj: [String:Any] = ["firmware":"DEMOSTRACIÓN", "boot_id":1,"profileGeneration":1,
            "clockValid":true,"clock":"Demostración", "rgb":[240,238,230],"brightness":20,"chLiveRgb":[65,60,56],
            "points":CieloProfile.wire(p.lights.pezverde.points),"chiPoints":CieloProfile.wire(p.lights.chihiros.points),
            "cieloV4":["pv_link_connected":true,"ch_link_connected":true,"pv_online_recent":true,"ch_online_recent":true],
            "health":["boot_ready":true,"current_critical":false],
            "work_light":["active":demoPhase != "idle","preparing":false,"can_start":demoPhase == "idle","phase":demoPhase,"remaining_sec":0]]
        return try JSONDecoder().decode(AtomStatus.self,from:JSONSerialization.data(withJSONObject:obj))
    }
    func refresh() async {
        guard !loading && !busy else { return }
        let token = revision; loading = true
        defer { if token == revision { loading = false } }
        do {
            let next: AtomStatus
            if demo { next = try demoStatus() }
            else { next = try JSONDecoder().decode(AtomStatus.self,from:await request("/api/status")) }
            guard token == revision else { return }
            status = next; updated = Date()
            if !dirty { profile = try CieloProfile.from(next); generation = next.profileGeneration; draftBoot = next.boot_id }
        } catch { if token == revision { updated = nil; message = error.localizedDescription } }
    }
    func replaceProfile(_ value: CieloProfile) {
        do { try value.validate(); profile = value; dirty = true }
        catch { message = error.localizedDescription }
    }
    func discard() { dirty = false; Task { await refresh() } }
    func save() async {
        guard canSave, var value = profile, let expected = generation else { return }
        guard status?.boot_id == draftBoot, status?.profileGeneration == generation else { message = CieloError.changed.localizedDescription; return }
        revision = UUID(); loading = false; busy = true
        do {
            value.seal(); try value.validate()
            if !demo {
                let data = try await request("/api/curves/commit",form:value.form(generation:expected))
                struct Commit: Decodable { var ok: Bool; var generation: Int }
                let commit = try JSONDecoder().decode(Commit.self,from:data)
                guard commit.ok else { throw CieloError.unavailable }; generation = commit.generation
            }
            if demo { demoSaved = value }
            dirty = false; message = demo ? "Perfil guardado en la demostración." : "Perfil guardado en el ATOM."
        } catch { updated = nil; message = error.localizedDescription + " El borrador se conserva. Consulta el estado antes de repetir." }
        busy = false; await refresh()
    }
    func maintenance(start: Bool) async {
        guard fresh && !busy else { return }
        revision = UUID(); loading = false; busy = true
        do {
            if demo { demoPhase = start ? "holding" : "idle" }
            else { _ = try await request(start ? "/api/work-light/start" : "/api/work-light/stop",form:start ? ["confirm":"INICIAR_MANTENIMIENTO"] : ["stop":"1"]) }
        } catch { updated = nil; message = error.localizedDescription + " Comprueba el estado antes de repetir." }
        busy = false; await refresh()
    }
    func downloadReport() async {
        guard fresh && !busy else { return }; busy = true; reportURL = nil
        defer { busy = false }
        do {
            let data: Data
            if demo { data = try JSONSerialization.data(withJSONObject:["demo":true,"optically_verified":false,"note":"Datos simulados de CIELO para revisar la app."],options:.prettyPrinted) }
            else { data = try await request("/api/diagnostico/maestro?share=1") }
            _ = try JSONSerialization.jsonObject(with:data)
            let url = FileManager.default.temporaryDirectory.appendingPathComponent("CIELO_INFORME_" + UUID().uuidString + ".json")
            try data.write(to:url,options:[.atomic,.completeFileProtection]); reportURL = url
        } catch { message = error.localizedDescription }
    }
}
