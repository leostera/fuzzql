open Lwt.Infix;

let respond_with_error = (reqd, err) => {
  let res =
    Httpaf.Response.create(
      ~headers=
        Httpaf.Headers.of_list([
          ("Content-Length", err |> String.length |> string_of_int),
        ]),
      `Internal_server_error,
    );
  Logs.err(m => m("%s", err));
  Httpaf.Reqd.respond_with_string(reqd, res, err);
  Lwt.return_unit;
};

let read_body = reqd => {
  open Httpaf;
  let (next, awake) = Lwt.wait();

  Lwt.async(() => {
    let body = reqd |> Reqd.request_body;
    let body_str = ref("");
    let on_eof = () => Lwt.wakeup_later(awake, body_str^);
    let rec on_read = (request_data, ~off, ~len) => {
      let read = Httpaf.Bigstring.to_string(~off, ~len, request_data);
      body_str := body_str^ ++ read;
      Body.schedule_read(body, ~on_read, ~on_eof);
    };
    Body.schedule_read(body, ~on_read, ~on_eof);
    Lwt.return_unit;
  });

  next;
};

let handle_request = (reqd, schema, ctx) => {
  open Httpaf;
  let req = reqd |> Reqd.request;
  Logs.debug(m =>
    m("%s %s", req.meth |> Httpaf.Method.to_string, req.target)
  );
  switch (req.meth, req.target) {
  | (`POST, "/graphql") =>
    /* 1. Get the body */
    reqd
    |> read_body
    >>= (
      body => {
        let query =
          Yojson.Basic.(
            body |> from_string |> Util.member("query") |> Util.to_string
          );
        Logs.debug(m => m("Query: %s", query));
        /* 2. Parse the body into a GraphQL Document */
        switch (Graphql_parser.parse(query)) {
        | Ok(doc) =>
          Logs.debug(m => m("Successfully parsed query!"));
          /* 3. Execute the Document with the Ctx and the Schema */
          switch (Graphql.Schema.execute(schema, ctx, doc)) {
          | Ok(`Response(data)) =>
            Logs.debug(m => m("Succesfully executed query!"));
            /* 4. Serialize the Resulting Value */
            let json_str = data |> Yojson.Basic.to_string;
            /* 5. Respond with serialized resulting value */
            let res =
              Response.create(
                ~headers=
                  Headers.of_list([
                    (
                      "Content-Length",
                      json_str |> String.length |> string_of_int,
                    ),
                  ]),
                `OK,
              );
            Httpaf.Reqd.respond_with_string(reqd, res, json_str);
            Lwt.return_unit;
          | Ok(`Stream(_)) => Lwt.return_unit
          | Error(err) =>
            let err_str = err |> Yojson.Basic.to_string;
            respond_with_error(reqd, err_str);
          };
        | Error(err) => respond_with_error(reqd, err)
        };
      }
    )
  | (_, _) =>
    let res =
      Response.create(
        ~headers=Headers.of_list([("Content-Length", "0")]),
        `Not_implemented,
      );
    Httpaf.Reqd.respond_with_string(reqd, res, "");
    Lwt.return_unit;
  };
};

let connection_handler:
  (_, _, Unix.sockaddr, Lwt_unix.file_descr) => Lwt.t(unit) =
  (schema, ctx) =>
    Httpaf_lwt.Server.create_connection_handler(
      ~config=?None,
      ~request_handler=
        (_client, reqd) =>
          Lwt.async(() => handle_request(reqd, schema, ctx)),
      ~error_handler=
        (_client, ~request as _=?, error, start_response) => {
          open Httpaf;
          /** Originally from: https://github.com/anmonteiro/reason-graphql-experiment/blob/master/src/server/httpaf_server.re#L56-L118 */
          let response_body = start_response(Headers.empty);

          switch (error) {
          | `Exn(exn) =>
            Body.write_string(response_body, Printexc.to_string(exn));
            Body.write_string(response_body, "\n");
          | `Bad_gateway as error
          | `Bad_request as error
          | `Internal_server_error as error =>
            Body.write_string(
              response_body,
              Httpaf.Status.default_reason_phrase(error),
            )
          };

          Body.close_writer(response_body);
        },
    );

let start = (port, schema, ctx) => {
  let listening_address = Unix.ADDR_INET(Unix.inet_addr_loopback, port);

  /* Set up the http server */
  let _ =
    Lwt_io.establish_server_with_client_socket(
      listening_address,
      /* Pass in the schema and context so the response handlers */
      connection_handler(schema, ctx),
    )
    >>= (
      /* can use it */
      _ =>
        Logs_lwt.app(m => m("Server running at http://localhost:%d", port))
    );

  /* Keep the server running */
  let (forever, _) = Lwt.wait();
  forever |> Lwt_main.run;
};
