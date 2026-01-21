# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AppGroup.qc provides the AppGroup namespace for data provider application grouping

/** AppGroup.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

/** @defgroup dp_app_groups DataProvider Application Groups

    Standard application group names for categorizing data provider applications.

    Use these constants when registering apps with DataProviderActionCatalog::registerApp()
    to ensure consistent categorization across all data providers.

    Access as @code AppGroup::Messaging @endcode
*/
#/@{
#! Data Provider Application Group namespace
/** Provides type-safe application group names for categorizing data provider applications
*/
public namespace AppGroup {
    #! Accounting & ERP systems
    public const AccountingErp = "Accounting & ERP";
    #! AI & Language Models
    public const AiLlm = "AI & Language Models";
    #! Analytics & Reporting tools
    public const Analytics = "Analytics & Reporting";
    #! Cloud Storage & File Management
    public const CloudStorage = "Cloud Storage & File Management";
    #! CRM & Sales Management
    public const CrmSales = "CRM & Sales Management";
    #! Customer Support & Helpdesk
    public const CustomerSupport = "Customer Support & Helpdesk";
    #! Databases & Backend Services
    public const Databases = "Databases & Backend Services";
    #! Design & Creative Tools
    public const DesignCreative = "Design & Creative Tools";
    #! DevOps & Cloud Infrastructure
    public const DevOps = "DevOps & Cloud Infrastructure";
    #! Document Signing & Contracts
    public const DocumentSigning = "Document Signing & Contracts";
    #! Documents & Documentation
    public const Documents = "Documents & Documentation";
    #! E-commerce Platforms
    public const Ecommerce = "E-commerce Platforms";
    #! Email & Email Marketing
    public const Email = "Email & Email Marketing";
    #! Forms, Surveys & Scheduling
    public const FormsSurveys = "Forms, Surveys & Scheduling";
    #! Google Workspace Suite
    public const GoogleWorkspace = "Google Workspace Suite";
    #! HR & People Management
    public const Hr = "HR & People Management";
    #! Marketing Automation
    public const Marketing = "Marketing Automation";
    #! Messaging & Real-time Communication
    public const Messaging = "Messaging & Real-time Communication";
    #! Notifications & Alerts
    public const Notifications = "Notifications & Alerts";
    #! Payment Processing
    public const Payments = "Payment Processing";
    #! Project & Task Management
    public const ProjectManagement = "Project & Task Management";
    #! Social Media Management
    public const SocialMedia = "Social Media Management";
    #! Spreadsheets & Data Tables
    public const Spreadsheets = "Spreadsheets & Data Tables";
    #! Version Control & Code Repositories
    public const VersionControl = "Version Control & Code Repositories";
    #! Video Conferencing & Meetings
    public const VideoConferencing = "Video Conferencing & Meetings";
    #! Weather services
    public const Weather = "Weather";
    #! Web Scraping & Automation
    public const WebAutomation = "Web Scraping & Automation";
    #! Hospitality & Property Management
    public const Hospitality = "Hospitality & Property Management";
    #! IoT & Smart Building
    public const Iot = "IoT & Smart Building";
    #! Data Transformation utilities
    public const DataTransformation = "Data Transformation";
    #! File System & Local Storage
    public const FileSystem = "File System & Local Storage";
    #! File Transfer Protocols (FTP, SFTP, etc.)
    public const FileTransfer = "File Transfer Protocols";
    #! API & Integration tools
    public const ApiIntegration = "API & Integration";
}
#/@}
