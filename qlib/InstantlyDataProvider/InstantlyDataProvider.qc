# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! The main Instantly data provider class (root provider)
public class InstantlyDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "instantly",
            "desc": "Instantly data provider for email outreach and sales automation",
            "type": "InstantlyDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection/List providers
            "accounts": Class::forName("InstantlyAccountsDataProvider"),
            "campaigns": Class::forName("InstantlyCampaignsDataProvider"),
            "campaign-subsequences": Class::forName("InstantlyCampaignSubsequencesDataProvider"),
            "leads": Class::forName("InstantlyLeadsDataProvider"),
            "lead-lists": Class::forName("InstantlyLeadListsDataProvider"),
            "lead-labels": Class::forName("InstantlyLeadLabelsDataProvider"),
            "emails": Class::forName("InstantlyEmailsDataProvider"),
            "block-list": Class::forName("InstantlyBlockListDataProvider"),
            "custom-tags": Class::forName("InstantlyCustomTagsDataProvider"),
            "inbox-placement-tests": Class::forName("InstantlyInboxPlacementTestsDataProvider"),
            "ip-analytics": Class::forName("InstantlyListIPAnalyticsDataProvider"),
            "ip-blacklist-reports": Class::forName("InstantlyListIPBlacklistReportsDataProvider"),
            "background-jobs": Class::forName("InstantlyBackgroundJobsDataProvider"),
            "workspace-members": Class::forName("InstantlyWorkspaceMembersDataProvider"),
            "workspace-group-members": Class::forName("InstantlyWorkspaceGroupMembersDataProvider"),
            "audit-logs": Class::forName("InstantlyAuditLogsDataProvider"),

            # Action providers - Accounts
            "create-account": Class::forName("InstantlyCreateAccountDataProvider"),
            "get-account": Class::forName("InstantlyGetAccountDataProvider"),
            "patch-account": Class::forName("InstantlyPatchAccountDataProvider"),
            "delete-account": Class::forName("InstantlyDeleteAccountDataProvider"),
            "pause-account": Class::forName("InstantlyPauseAccountDataProvider"),
            "resume-account": Class::forName("InstantlyResumeAccountDataProvider"),
            "mark-account-fixed": Class::forName("InstantlyMarkAccountFixedDataProvider"),
            "test-account-vitals": Class::forName("InstantlyTestAccountVitalsDataProvider"),
            "get-warmup-analytics": Class::forName("InstantlyGetWarmupAnalyticsDataProvider"),
            "get-custom-tracking-domain-status": Class::forName("InstantlyGetCustomTrackingDomainStatusDataProvider"),

            # Action providers - Campaigns
            "create-campaign": Class::forName("InstantlyCreateCampaignDataProvider"),
            "get-campaign": Class::forName("InstantlyGetCampaignDataProvider"),
            "patch-campaign": Class::forName("InstantlyPatchCampaignDataProvider"),
            "delete-campaign": Class::forName("InstantlyDeleteCampaignDataProvider"),
            "update-campaign-status": Class::forName("InstantlyUpdateCampaignStatusDataProvider"),
            "share-campaign": Class::forName("InstantlyShareCampaignDataProvider"),
            "get-campaign-analytics": Class::forName("InstantlyGetCampaignAnalyticsDataProvider"),
            "get-campaign-analytics-overview": Class::forName("InstantlyGetCampaignAnalyticsOverviewDataProvider"),
            "get-campaign-daily-analytics": Class::forName("InstantlyGetCampaignDailyAnalyticsDataProvider"),
            "get-campaign-step-analytics": Class::forName("InstantlyGetCampaignStepAnalyticsDataProvider"),

            # Action providers - Campaign Subsequences
            "create-campaign-subsequence": Class::forName("InstantlyCreateCampaignSubsequenceDataProvider"),
            "get-campaign-subsequence": Class::forName("InstantlyGetCampaignSubsequenceDataProvider"),
            "patch-campaign-subsequence": Class::forName("InstantlyPatchCampaignSubsequenceDataProvider"),
            "delete-campaign-subsequence": Class::forName("InstantlyDeleteCampaignSubsequenceDataProvider"),
            "pause-subsequence": Class::forName("InstantlyPauseCampaignSubsequenceDataProvider"),
            "resume-subsequence": Class::forName("InstantlyResumeCampaignSubsequenceDataProvider"),
            "duplicate-subsequence": Class::forName("InstantlyDuplicateCampaignSubsequenceDataProvider"),

            # Action providers - Leads
            "create-lead": Class::forName("InstantlyCreateLeadDataProvider"),
            "get-lead": Class::forName("InstantlyGetLeadDataProvider"),
            "patch-lead": Class::forName("InstantlyPatchLeadDataProvider"),
            "delete-lead": Class::forName("InstantlyDeleteLeadDataProvider"),
            "merge-leads": Class::forName("InstantlyMergeLeadsDataProvider"),
            "move-leads": Class::forName("InstantlyMoveLeadsDataProvider"),
            "move-lead-to-subsequence": Class::forName("InstantlyMoveLeadToSubsequenceDataProvider"),
            "remove-lead-from-subsequence": Class::forName("InstantlyRemoveLeadFromSubsequenceDataProvider"),
            "update-lead-interest-status": Class::forName("InstantlyUpdateLeadInterestStatusDataProvider"),
            "bulk-assign-leads": Class::forName("InstantlyBulkAssignLeadsDataProvider"),
            "export-leads": Class::forName("InstantlyExportLeadsDataProvider"),

            # Action providers - Lead Lists
            "create-lead-list": Class::forName("InstantlyCreateLeadListDataProvider"),
            "get-lead-list": Class::forName("InstantlyGetLeadListDataProvider"),
            "patch-lead-list": Class::forName("InstantlyPatchLeadListDataProvider"),
            "delete-lead-list": Class::forName("InstantlyDeleteLeadListDataProvider"),

            # Action providers - Lead Labels
            "create-lead-label": Class::forName("InstantlyCreateLeadLabelDataProvider"),
            "get-lead-label": Class::forName("InstantlyGetLeadLabelDataProvider"),
            "patch-lead-label": Class::forName("InstantlyPatchLeadLabelDataProvider"),
            "delete-lead-label": Class::forName("InstantlyDeleteLeadLabelDataProvider"),

            # Action providers - Emails
            "get-email": Class::forName("InstantlyGetEmailDataProvider"),
            "patch-email": Class::forName("InstantlyPatchEmailDataProvider"),
            "delete-email": Class::forName("InstantlyDeleteEmailDataProvider"),
            "reply-to-email": Class::forName("InstantlyReplyToEmailDataProvider"),
            "count-unread-emails": Class::forName("InstantlyCountUnreadEmailsDataProvider"),
            "mark-thread-as-read": Class::forName("InstantlyMarkThreadAsReadDataProvider"),
            "get-campaign-for-email": Class::forName("InstantlyGetCampaignForEmailDataProvider"),

            # Action providers - Block List
            "create-block-list-entry": Class::forName("InstantlyCreateBlockListEntryDataProvider"),
            "delete-block-list-entry": Class::forName("InstantlyDeleteBlockListEntryDataProvider"),
            "patch-block-list": Class::forName("InstantlyPatchBlockListDataProvider"),

            # Action providers - Custom Tags
            "create-custom-tag": Class::forName("InstantlyCreateCustomTagDataProvider"),
            "get-custom-tag": Class::forName("InstantlyGetCustomTagDataProvider"),
            "patch-custom-tag": Class::forName("InstantlyPatchCustomTagDataProvider"),
            "delete-custom-tag": Class::forName("InstantlyDeleteCustomTagDataProvider"),
            "assign-custom-tag": Class::forName("InstantlyAssignCustomTagDataProvider"),

            # Action providers - Inbox Placement Tests
            "create-inbox-placement-test": Class::forName("InstantlyCreateInboxPlacementTestDataProvider"),
            "get-inbox-placement-test": Class::forName("InstantlyGetInboxPlacementTestDataProvider"),
            "patch-inbox-placement-test": Class::forName("InstantlyPatchInboxPlacementTestDataProvider"),
            "delete-inbox-placement-test": Class::forName("InstantlyDeleteInboxPlacementTestDataProvider"),

            # Action providers - Inbox Placement Analytics
            "get-inbox-placement-analytics": Class::forName("InstantlyGetInboxPlacementAnalyticsDataProvider"),
            "get-ip-analytics-stats-by-date": Class::forName("InstantlyGetIPAnalyticsStatsByDateDataProvider"),
            "get-ip-analytics-stats-by-test": Class::forName("InstantlyGetIPAnalyticsStatsByTestDataProvider"),
            "get-ip-deliverability-insights": Class::forName("InstantlyGetIPDeliverabilityInsightsDataProvider"),
            "get-ip-blacklist-report": Class::forName("InstantlyGetIPBlacklistReportDataProvider"),

            # Action providers - Email Verification
            "create-email-verification": Class::forName("InstantlyCreateEmailVerificationDataProvider"),
            "get-email-verification-status": Class::forName("InstantlyGetEmailVerificationStatusDataProvider"),

            # Action providers - Background Jobs
            "get-background-job": Class::forName("InstantlyGetBackgroundJobDataProvider"),

            # Action providers - Workspace
            "get-workspace": Class::forName("InstantlyGetWorkspaceDataProvider"),
            "get-workspace-admin": Class::forName("InstantlyGetWorkspaceAdminDataProvider"),
            "patch-workspace": Class::forName("InstantlyPatchWorkspaceDataProvider"),

            # Action providers - Workspace Members
            "create-workspace-member": Class::forName("InstantlyCreateWorkspaceMemberDataProvider"),
            "get-workspace-member": Class::forName("InstantlyGetWorkspaceMemberDataProvider"),
            "patch-workspace-member": Class::forName("InstantlyPatchWorkspaceMemberDataProvider"),
            "delete-workspace-member": Class::forName("InstantlyDeleteWorkspaceMemberDataProvider"),

            # Action providers - Workspace Group Members
            "create-workspace-group-member": Class::forName("InstantlyCreateWorkspaceGroupMemberDataProvider"),
            "get-workspace-group-member": Class::forName("InstantlyGetWorkspaceGroupMemberDataProvider"),
            "delete-workspace-group-member": Class::forName("InstantlyDeleteWorkspaceGroupMemberDataProvider"),

            # Event providers
            "events": Class::forName("InstantlyEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : InstantlyDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(InstantlyRestClient::InstantlyRestConnection conn) : InstantlyDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "instantly";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Instantly data provider for email outreach and sales automation";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
