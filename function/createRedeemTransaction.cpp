transaction createAtomicSwapTetherRedeemTransaction(const ec_private& Alice_private,
                                                     const ec_public& Bob_pubkey, const ec_private& Bob_private, const uint64_t& tether_amount,
                                                     const uint32_t& locktime, const data_chunk& swap_secret,
                                                    const transaction& Funding_tx)
{

    payment_address Alice_address=Alice_private.to_payment_address();
    data_chunk swap_secret_hash=ripemd160_hash_chunk(swap_secret);


    transaction redeem_tx;

    redeem_tx.set_version(1u);

    //входы транзакции(в идеале лучше было бы, еслиб программа сама находила и строила входы, как например с помощью функции getUTXO, суть ее в том, чтобы обраться к серверу(в данном случае libbitcoin серверу) и отправить запрос на получение данных из блокчейна, из которые уже можно бы было получить UTXO для конкретного адреса,но сервер возвращает пустое множество UTXO, для любого адреса. долгое время не смогя разобраться с этой проблемой, было решено пока возложить на пользователя задачу найти и вписать свой UTXO)
    //выходы можно самостоятельно посмотреть на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //тут необходимо ввести хэш транзакции в которой есть непотраченный выход
    std::string PrevTxHash_str;
    std::cout<<"\n write your prev UTXO hash in base16, once output from this will input for channel's creation's transaction:\n";
    std::cin>>PrevTxHash_str;
    hash_digest PrevTxHash;
    decode_hash(PrevTxHash, PrevTxHash_str);

    //тут нужно ввести индекс непотраченного выхода, индексация начинается с нуля, выходы например на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //расположены сверху внизу в порядке увеличения их индекса
    uint32_t PrevTxIndex;
    std::cout<<"\n write index unspended output of your UTXO, its output will be input:\n";
    std::cin>>PrevTxIndex;


    //сдача пользователю, то есть открывающая транзакция (opening_tx) имеет 2 выхода - первый, на счет с мультподписью размер отправляеных на него биткоинов равен ширине канала, второй - сдача пользователю, остаток средств которые он хочет вернуть на свой адрес, он наберет их сам, учитывая какую сумму он хочет потратить на fees
    std::string OddMoney_btc;
    uint64_t OddMoney_satoshi;
    std::cout<<"\n write odd money (in BTC), for creating p2pkh output on your address. Dont forgot about transaction's fees:\n";
    std::cin>>OddMoney_btc;
    decode_base10(OddMoney_satoshi,OddMoney_btc, btc_decimal_places);

    output_point UTXO(PrevTxHash, PrevTxIndex);
    input input0;
    input0.set_previous_output(UTXO);
    input0.set_sequence(0xffffffff);

    input input1;
    output_point FundingTxOutput(Funding_tx.hash(), 0u);
    input1.set_previous_output(FundingTxOutput);
    input1.set_sequence(0x00000000);

    redeem_tx.inputs().push_back(input0); //добавим вход без подписи
    redeem_tx.inputs().push_back(input1); //добавим вход без подписи


    //выходы


    //выход с op_return скриптом
    operation::list OmniScript;
    OmniScript.push_back(operation(opcode::return_));
    data_chunk omni_payload;
    omni_payload.push_back(0x6f); //o
    omni_payload.push_back(0x6d); //m
    omni_payload.push_back(0x6e); //n
    omni_payload.push_back(0x69); //i

    //version
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0

    //type =0 ("simple send")
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0

    //token identifier =31 (TetherUS)
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x1f); //1

    //amount
    for(int i=7; i>=0; i--)
    {
       omni_payload.push_back( tether_amount>>(8*i));
    }

    OmniScript.push_back( operation(omni_payload) );
    output Output0(0u, OmniScript); //создаем второй выход

    //выход на адрес боба
    operation::list p2pkhScript0=script::to_pay_key_hash_pattern(Bob_pubkey.to_payment_address().hash()); //скрипт для возвращает сдачу себе
    output Output1(SATOSHI_FOR_OMNI_OUTPUT, p2pkhScript0); //создаем второй выход

    //выход на адрес алисы
    operation::list p2pkhScript1=script::to_pay_key_hash_pattern(Alice_address.hash()); //скрипт для возвращает сдачу себе
    output Output2(OddMoney_satoshi, p2pkhScript1); //создаем второй выход


    redeem_tx.outputs().push_back(Output0);
    redeem_tx.outputs().push_back(Output1);
    redeem_tx.outputs().push_back(Output2);



    operation::list SwapScript;

    SwapScript.push_back( operation(opcode::ripemd160) );
    SwapScript.push_back( operation(swap_secret_hash) );
    SwapScript.push_back( operation(opcode::equal) );

    SwapScript.push_back( operation(opcode::if_) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey.point())) );

    SwapScript.push_back( operation(opcode::else_) );
    SwapScript.push_back( operation(uint32_to_data_chunk_inverse(locktime)) );
    SwapScript.push_back( operation(opcode::checklocktimeverify) );
    SwapScript.push_back( operation(opcode::drop) );
    SwapScript.push_back( operation(to_chunk(Alice_private.to_public().point() )) );

    SwapScript.push_back( operation(opcode::endif) );
    SwapScript.push_back( operation(opcode::checksig) );

    script redeem_script(SwapScript);

    //подпишемся!
    endorsement Sig0;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
    script::create_endorsement(Sig0, Alice_private.secret(), p2pkhScript1, redeem_tx, 0u, sighash_algorithm::all);

    operation::list  sig_script0;
    sig_script0.push_back(operation(Sig0));
    sig_script0.push_back(operation(to_chunk(Alice_private.to_public().point() )));
    script InputScript0(sig_script0);
    redeem_tx.inputs()[0].set_script(InputScript0);

    std::cout<<"REDEEM TRANSACTION WITHOUT BOB'S SIGN:\n"<<encode_base16( redeem_tx.to_data() )<<std::endl;

    endorsement Sig1;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
    script::create_endorsement(Sig1, Bob_private.secret(), redeem_script , redeem_tx, 1u, sighash_algorithm::all);

    operation::list  sig_script1;
    sig_script1.push_back(operation(Sig1));
    sig_script1.push_back(operation( swap_secret ));
    sig_script1.push_back( operation(to_chunk(redeem_script.to_data(false)) ));



    script InputScript1(sig_script1);
    redeem_tx.inputs()[1].set_script(InputScript1);


    return redeem_tx;

}
