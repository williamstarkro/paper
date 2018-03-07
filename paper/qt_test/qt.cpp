#include <gtest/gtest.h>

#include <paper/qt/qt.hpp>
#include <thread>
#include <QTest>

TEST (client, construction)
{
    paper::system system (24000, 1);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
}

TEST (client, main)
{
    paper::system system (24000, 1);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
    ASSERT_EQ (client.entry_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.send_blocks, Qt::LeftButton);
    ASSERT_EQ (client.send_blocks_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.send_blocks_back, Qt::LeftButton);
    ASSERT_EQ (client.entry_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.settings, Qt::LeftButton);
    ASSERT_EQ (client.settings_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.settings_change_password_button, Qt::LeftButton);
    ASSERT_EQ (client.password_change.window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.password_change.back, Qt::LeftButton);
    ASSERT_EQ (client.settings_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.settings_back, Qt::LeftButton);
    ASSERT_EQ (client.entry_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.show_advanced, Qt::LeftButton);
    ASSERT_EQ (client.advanced.window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.advanced.show_ledger, Qt::LeftButton);
    ASSERT_EQ (client.advanced.ledger_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.advanced.ledger_back, Qt::LeftButton);
    ASSERT_EQ (client.advanced.window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.advanced.show_peers, Qt::LeftButton);
    ASSERT_EQ (client.advanced.peers_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.advanced.peers_back, Qt::LeftButton);
    ASSERT_EQ (client.advanced.window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.advanced.show_log, Qt::LeftButton);
    ASSERT_EQ (client.advanced.log_window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.advanced.log_back, Qt::LeftButton);
    ASSERT_EQ (client.advanced.window, client.main_stack->currentWidget ());
    QTest::mouseClick (client.advanced.back, Qt::LeftButton);
    ASSERT_EQ (client.entry_window, client.main_stack->currentWidget ());
}

TEST (client, password_change)
{
    paper::system system (24000, 1);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
    QTest::mouseClick (client.settings, Qt::LeftButton);
    QTest::mouseClick (client.settings_change_password_button, Qt::LeftButton);
    ASSERT_NE (client.client_m.wallet.derive_key ("1"), client.client_m.wallet.password.value ());
    QTest::keyClicks (client.password_change.password, "1");
    QTest::keyClicks (client.password_change.retype, "1");
    QTest::mouseClick (client.password_change.change, Qt::LeftButton);
    ASSERT_EQ (client.client_m.wallet.derive_key ("1"), client.client_m.wallet.password.value ());
    ASSERT_EQ ("", client.password_change.password->text ());
    ASSERT_EQ ("", client.password_change.retype->text ());
}

TEST (client, password_nochange)
{
    paper::system system (24000, 1);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
    QTest::mouseClick (client.settings, Qt::LeftButton);
    QTest::mouseClick (client.settings_change_password_button, Qt::LeftButton);
    ASSERT_EQ (client.client_m.wallet.derive_key (""), client.client_m.wallet.password.value ());
    QTest::keyClicks (client.password_change.password, "1");
    QTest::keyClicks (client.password_change.retype, "2");
    QTest::mouseClick (client.password_change.change, Qt::LeftButton);
    ASSERT_EQ (client.client_m.wallet.derive_key (""), client.client_m.wallet.password.value ());
    ASSERT_EQ ("1", client.password_change.password->text ());
    ASSERT_EQ ("2", client.password_change.retype->text ());
}

TEST (client, enter_password)
{
    paper::system system (24000, 1);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
    ASSERT_NE (-1, client.enter_password.layout->indexOf (client.enter_password.valid));
    ASSERT_NE (-1, client.enter_password.layout->indexOf (client.enter_password.password));
    ASSERT_NE (-1, client.enter_password.layout->indexOf (client.enter_password.unlock));
    ASSERT_NE (-1, client.enter_password.layout->indexOf (client.enter_password.lock));
    ASSERT_NE (-1, client.enter_password.layout->indexOf (client.enter_password.back));
    ASSERT_FALSE (client.client_m.wallet.rekey ("abc"));
    QTest::mouseClick (client.settings, Qt::LeftButton);
    QTest::mouseClick (client.settings_enter_password_button, Qt::LeftButton);
    QTest::keyClicks (client.enter_password.password, "a");
    QTest::mouseClick (client.enter_password.unlock, Qt::LeftButton);
    ASSERT_EQ ("Password: INVALID", client.enter_password.valid->text ());
    client.enter_password.password->setText ("");
    QTest::keyClicks (client.enter_password.password, "abc");
    QTest::mouseClick (client.enter_password.unlock, Qt::LeftButton);
    ASSERT_EQ ("Password: Valid", client.enter_password.valid->text ());
    ASSERT_EQ ("", client.enter_password.password->text ());
}

TEST (client, send)
{
    paper::system system (24000, 2);
    system.clients [0]->wallet.insert (paper::test_genesis_key.prv);
    paper::keypair key1;
    std::string account;
    key1.pub.encode_base58check (account);
    system.clients [1]->wallet.insert (key1.prv);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
    QTest::mouseClick (client.send_blocks, Qt::LeftButton);
    QTest::keyClicks (client.send_address, account.c_str ());
    QTest::keyClicks (client.send_count, "2");
    QTest::mouseClick (client.send_blocks_send, Qt::LeftButton);
    while (client.client_m.ledger.account_balance (key1.pub).is_zero ())
    {
        system.service->poll_one ();
        system.processor.poll_one ();
    }
	ASSERT_EQ (2 * client.advanced.scale, client.client_m.ledger.account_balance (key1.pub));
	QTest::mouseClick (client.send_blocks_back, Qt::LeftButton);
    QTest::mouseClick (client.show_advanced, Qt::LeftButton);
	QTest::mouseClick (client.advanced.show_ledger, Qt::LeftButton);
	QTest::mouseClick (client.advanced.ledger_refresh, Qt::LeftButton);
	ASSERT_EQ (2, client.advanced.ledger_model->rowCount ());
	ASSERT_EQ (3, client.advanced.ledger_model->columnCount ());
	auto item (client.advanced.ledger_model->itemFromIndex (client.advanced.ledger_model->index (1, 1)));
	ASSERT_EQ ("2", item->text ().toStdString ());
}

TEST (client, scaling)
{
    paper::system system (24000, 1);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
    auto max (std::numeric_limits <paper::uint128_t>::max ());
    auto down (client.advanced.scale_down (max));
    auto up1 (client.advanced.scale_up (down));
    auto up2 (client.advanced.scale_up (down - 1));
    ASSERT_LT (up2, up1);
    ASSERT_EQ (up1 - up2, client.advanced.scale);
}

TEST (client, scale_num)
{
    paper::system system (24000, 1);
    int argc (0);
    QApplication application (argc, nullptr);
    paper_qt::client client (application, *system.clients [0]);
    paper::uint128_t num ("100000000000000000000000000000000000000");
    auto down (client.advanced.scale_down (num));
    auto up (client.advanced.scale_up (down));
    ASSERT_EQ (num, up);
}