import Foundation

final class MockTransport: URLProtocol {
    static var handler: ((URLRequest) throws -> (Int, Data))!
    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    override func startLoading() {
        do {
            let (code,data) = try Self.handler(request)
            let response = HTTPURLResponse(url:request.url!,statusCode:code,httpVersion:nil,headerFields:["Content-Type":"application/json"])!
            client?.urlProtocol(self,didReceive:response,cacheStoragePolicy:.notAllowed)
            client?.urlProtocol(self,didLoad:data);client?.urlProtocolDidFinishLoading(self)
        } catch { client?.urlProtocol(self,didFailWithError:error) }
    }
    override func stopLoading() {}
}

@main struct ControllerChecks {
    @MainActor static func main() async throws {
        let initial = try JSONDecoder().decode(CieloProfile.self,from:Data(contentsOf:URL(fileURLWithPath:CommandLine.arguments[1])))
        var remote = initial; var generation = 14; var postCount = 0; var failure: Int?; var lastBody = ""; var lastPath = ""
        MockTransport.handler = { request in
            if request.httpMethod == "POST" {
                postCount += 1;lastPath = request.url!.path
                let body: Data
                if let data = request.httpBody { body = data }
                else if let stream = request.httpBodyStream {
                    stream.open();defer {stream.close()};var bytes = [UInt8](repeating:0,count:4096);var data=Data()
                    while stream.hasBytesAvailable { let n=stream.read(&bytes,maxLength:bytes.count);if n<=0 {break};data.append(bytes,count:n) };body=data
                } else {body=Data()}
                lastBody = String(decoding:body,as:UTF8.self)
                precondition(request.value(forHTTPHeaderField:"Origin") == "http://192.168.0.25")
                if let failure {
                    if failure == -1 {throw URLError(.timedOut)}
                    return (failure,Data("{\"ok\":false,\"error\":\"conflict\"}".utf8))
                }
                if lastPath == "/api/curves/commit" {
                    let fields=URLComponents(string:"http://local/?"+lastBody)!.queryItems!
                    let form=Dictionary(uniqueKeysWithValues:fields.map{($0.name,$0.value ?? "")})
                    precondition(form["generation"] == String(generation))
                    precondition(form["schema_version"] == "2")
                    remote.lights.pezverde.points = try CieloProfile.parseWire(form["pv"]!)
                    remote.lights.chihiros.points = try CieloProfile.parseWire(form["ch"]!)
                    remote.seal();try remote.validate();precondition(remote.computedChecksum == form["checksum"])
                    generation += 1
                }
                return (200,try JSONSerialization.data(withJSONObject:["ok":true,"generation":generation]))
            }
            precondition(request.url!.path == "/api/status")
            return (200,try JSONSerialization.data(withJSONObject:["firmware":"TEST","boot_id":142,"profileGeneration":generation,"points":CieloProfile.wire(remote.lights.pezverde.points),"chiPoints":CieloProfile.wire(remote.lights.chihiros.points),"work_light":["active":false,"preparing":false,"can_start":true,"phase":"idle"]]))
        }
        let config=URLSessionConfiguration.ephemeral;config.protocolClasses=[MockTransport.self]
        let model=CieloController(session:URLSession(configuration:config))
        await model.connect(host:"192.168.0.25",demo:false)
        precondition(model.fresh && model.profile?.computedChecksum == initial.computedChecksum && postCount == 0)
        var edited=initial;edited.lights.pezverde.points[1][1] = (edited.lights.pezverde.points[1][1]+1)%256;edited.seal()
        model.replaceProfile(edited);precondition(model.canSave)
        generation += 1;await model.refresh();await model.save()
        precondition(postCount == 0 && model.dirty && model.profile == edited)
        await model.connect(host:"192.168.0.25",demo:false);model.replaceProfile(edited)
        await model.save();precondition(postCount == 1 && !model.dirty && remote == edited && model.fresh)
        model.replaceProfile(initial);failure = -1;await model.save()
        precondition(postCount == 2 && model.dirty && model.profile == initial)
        await model.refresh();precondition(postCount == 2)
        failure = 409;await model.save();precondition(postCount == 3 && model.dirty)
        failure = nil;await model.maintenance(start:true)
        precondition(postCount == 4 && lastPath == "/api/work-light/start" && lastBody.contains("confirm=INICIAR_MANTENIMIENTO"))
        await model.maintenance(start:false)
        precondition(postCount == 5 && lastPath == "/api/work-light/stop")
        model.invalidate();precondition(!model.fresh && !model.canSave)
        print("PASS native controller: read-only connection, remote-generation conflict, atomic form, preserved drafts, no POST retries, maintenance commands and stale-state guard")
    }
}
