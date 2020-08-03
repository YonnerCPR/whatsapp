#include "WebView.hpp"
#include "Settings.hpp"
#include <locale>
#include <gtkmm/messagedialog.h>
#include <gtkmm/filechooserdialog.h>


namespace
{
    constexpr auto const WHATSAPP_WEB_URI = "https://web.whatsapp.com/";


    std::string systemLanguage()
    {
        auto lang = std::locale("").name();
        lang = lang.substr(0, lang.find('.'));
        return lang;
    }

    gboolean permissionRequest(WebKitWebView*, WebKitPermissionRequest* request, GtkWindow*)
    {
        // TODO Think about handling this in MainWindow by signaling and holding a ref to the request in WebView.
        auto dialog = Gtk::MessageDialog{"Notification Request", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO};
        dialog.set_secondary_text("Do you allow WhatsApp to send you notifications?");

        auto const result = dialog.run();
        switch (result)
        {
            case Gtk::RESPONSE_YES:
                webkit_permission_request_allow(request);
                Settings::instance().setAllowPermissions(true);
                break;
            case Gtk::RESPONSE_NO:
                webkit_permission_request_deny(request);
                Settings::instance().setAllowPermissions(false);
                break;
            default:
                break;
        }

        return TRUE;
    }

    gboolean decidePolicy(WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType decisionType, gpointer)
    {
        switch (decisionType)
        {
            case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
            {
                auto const navigationDecision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
                auto const navigationAction = webkit_navigation_policy_decision_get_navigation_action(navigationDecision);
                auto const request = webkit_navigation_action_get_request(navigationAction);
                auto const uri = webkit_uri_request_get_uri(request);

                GError* error = nullptr;
                gtk_show_uri_on_window(nullptr, uri, GDK_CURRENT_TIME, &error);
            } break;
            default:
                break;
        }

        return FALSE;
    }

    gboolean contextMenu(WebKitWebView*, WebKitContextMenu*, GdkEvent*, WebKitHitTestResult*, gpointer)
    {
        return FALSE;
    }

    void downloadStarted(WebKitWebContext*, WebKitDownload* download, gpointer)
    {
        auto dialog = Gtk::FileChooserDialog{"Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER};
        dialog.add_button("Select", Gtk::RESPONSE_OK);
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);

        int result = dialog.run();
        switch(result)
        {
            case Gtk::RESPONSE_OK:
                webkit_download_set_destination(download, dialog.get_filename().c_str());
                break;
            case Gtk::RESPONSE_CANCEL:
            default:
                break;
        }
    }

    void initializeNotificationPermission(WebKitWebContext* context, gpointer)
    {
        if (Settings::instance().allowPermissions())
        {
            auto const origin = webkit_security_origin_new_for_uri(WHATSAPP_WEB_URI);
            auto const allowedOrigins = g_list_alloc();
            allowedOrigins->data = origin;

            webkit_web_context_initialize_notification_permissions(context, allowedOrigins, nullptr);
        }
    }
}


WebView::WebView()
    : Gtk::Widget{webkit_web_view_new()}
{
    auto const webContext = webkit_web_view_get_context(*this);

    g_signal_connect(*this, "permission-request", G_CALLBACK(permissionRequest), nullptr);
    g_signal_connect(*this, "decide-policy", G_CALLBACK(decidePolicy), nullptr);
    g_signal_connect(*this, "context-menu", G_CALLBACK(contextMenu), nullptr);
    g_signal_connect(webContext, "download-started", G_CALLBACK(downloadStarted), nullptr);
    g_signal_connect(webContext, "initialize-notification-permissions", G_CALLBACK(initializeNotificationPermission), nullptr);

    auto const lang = systemLanguage();
    gchar const* const spellCheckingLangs[] = {lang.c_str(), 0};
    webkit_web_context_set_spell_checking_languages(webContext, spellCheckingLangs);
    webkit_web_context_set_spell_checking_enabled(webContext, TRUE);

    auto const settings = webkit_web_view_get_settings(*this);
    webkit_settings_set_enable_developer_extras(settings, TRUE);

    webkit_web_view_load_uri(*this, WHATSAPP_WEB_URI);
}

WebView::operator WebKitWebView*()
{
    return WEBKIT_WEB_VIEW(gobj());
}

void WebView::refresh()
{
    webkit_web_view_reload(*this);
}
